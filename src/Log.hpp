#pragma once

#include <mutex>
#include <vector>
#include <sstream>

#define LOG(LEV) lldbg::LogMessageStream(lldbg::LogLevel::LEV)

namespace lldbg {

enum class LogLevel {
    Info,
    Debug,
    Warning,
    Error
};

struct LogMessage {
    const LogLevel level;
    const std::string message;

    LogMessage(LogLevel level, std::string&& message)
        : level(level), message(std::move(message)) {}
};

class Logger {
    std::mutex m_mutex;
    std::vector<LogMessage> m_messages;

public:
    void log(LogLevel level, std::string&& message) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_messages.emplace_back(level, std::move(message));
    };

    template <typename Callable>
    void for_each_message(Callable&& f) {
        std::unique_lock<std::mutex> lock(m_mutex);
        for (const LogMessage& message : m_messages) {
            f(message);
        }
    }
};

extern Logger g_logger;

class LogMessageStream {
    const LogLevel level;
    std::ostringstream oss;

public:
    template <typename T>
    LogMessageStream& operator<<(const T& data) {
        oss << data;
        return *this;
    }

    LogMessageStream(LogLevel level) : level(level) {}
    ~LogMessageStream() {
        oss << std::endl;
        g_logger.log(level, oss.str());
    };
};

}

