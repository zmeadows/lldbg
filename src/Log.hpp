#pragma once

#include <memory>
#include <mutex>
#include <sstream>
#include <vector>

// TODO: modify to include file/function/line information?
// also include lldb version/commit number?
#define LOG(LEV) lldbg::LogMessageStream(lldbg::LogLevel::LEV)

namespace lldbg {

enum class LogLevel { Verbose, Debug, Info, Warning, Error };

struct LogMessage {
    const LogLevel level;
    const std::string message;

    LogMessage(LogLevel level, const std::string& message) : level(level), message(message) {}
};

class Logger {
    std::vector<LogMessage> m_messages;

    static std::unique_ptr<Logger> s_instance;
    static std::mutex s_mutex;

    // TODO: use static methods for 'log' and 'for_each_message' which themselves
    // access the s_instance variable

public:
    static Logger* get_instance(void)
    {
        std::unique_lock<std::mutex> lock(s_mutex);
        if (!s_instance) {
            s_instance = std::make_unique<Logger>();
        }
        return s_instance.get();
    }

    void log(LogLevel level, const std::string& message)
    {
        std::unique_lock<std::mutex> lock(s_mutex);
        m_messages.emplace_back(level, message);
    };

    template <typename MessageHandlerFunc>
    void for_each_message(MessageHandlerFunc&& f)
    {
        std::unique_lock<std::mutex> lock(s_mutex);
        for (const LogMessage& message : m_messages) {
            f(message);
        }
    }
};

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
        Logger::get_instance()->log(level, oss.str());
    };
};

}  // namespace lldbg

