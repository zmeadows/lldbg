#pragma once

#include <memory>
#include <mutex>
#include <sstream>
#include <vector>

#define LOG(LEV) lldbg::LogMessageStream(lldbg::LogLevel::LEV)

namespace lldbg {

enum class LogLevel { Verbose, Debug, Info, Warning, Error };

struct LogMessage {
    const LogLevel level;
    const std::string message;

    LogMessage(LogLevel level, const std::string& message) : level(level), message(message) {}
};

class Logger {
    std::mutex m_mutex;
    std::vector<LogMessage> m_messages;

public:
    void log(LogLevel level, const std::string& message)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_messages.emplace_back(level, message);
    };

    template <typename MessageHandlerFunc>
    void for_each_message(MessageHandlerFunc&& f)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        for (const LogMessage& message : m_messages) {
            f(message);
        }
    }
};

extern std::unique_ptr<Logger> g_logger;

class LogMessageStream {
    const LogLevel level;
    std::ostringstream oss;

public:
    template <typename T>
    LogMessageStream& operator<<(const T& data)
    {
        oss << data;
        return *this;
    }

    LogMessageStream(LogLevel level) : level(level) {}

    ~LogMessageStream()
    {
        oss << std::endl;
        g_logger->log(level, oss.str());
    };
};

}  // namespace lldbg

