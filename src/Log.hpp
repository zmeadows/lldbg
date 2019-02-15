#pragma once

#include <mutex>
#include <vector>
#include <sstream>

#define LOG(LEV) lldbg::LogMessageStream(lldbg::LogLevel::LEV)

namespace lldbg {

enum class LogLevel {
    Verbose,
    Debug,
    Info,
    Warning,
    Error
};

struct LogMessage final {
    const LogLevel level;
    const std::string message;

    LogMessage(LogLevel level, const std::string& message)
        : level(level), message(message) {}
};

class Logger final {
    std::mutex m_mutex;
    std::vector<LogMessage> m_messages;

public:
    void log(LogLevel level, const std::string& message) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_messages.emplace_back(level, message);
    };

    //TODO: just do begin/end
    template <typename Callable>
    void for_each_message(Callable&& f) {
        std::unique_lock<std::mutex> lock(m_mutex);
        for (const LogMessage& message : m_messages) {
            f(message);
        }
    }
};

extern Logger g_logger;

class LogMessageStream final {
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

