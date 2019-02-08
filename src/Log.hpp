#pragma once

#include <mutex>
#include <vector>

namespace lldbg {

struct LogMessage {
    enum class Level {
        Info,
        Debug,
        Warning,
        Error
    };

    const LogMessage::Level level;
    const std::string message;

    LogMessage(Level level, const std::string& message) :
        level(level), message(message) {}
};

class Logger {
    std::mutex m_mutex;
    std::vector<LogMessage> m_messages;

    //TODO: add log file
public:
    void log(LogMessage&& message) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_messages.emplace_back(message);
    };
};

}
