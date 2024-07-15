#pragma once

#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

// TODO: modify to include file/function/line information?
// also include lldb version/commit number?
#define LOG(LEV) LogMessageStream(LogLevel::LEV)

enum class LogLevel {
    Debug = 80,
    Verbose = 60,
    Info = 40,
    Warning = 20,
    Error = 0,
};

struct LogMessage {
    const LogLevel level;
    const std::string message;

    LogMessage(LogLevel level, const std::string& message) : level(level), message(message) {}
};

class Logger {
    std::vector<LogMessage> m_messages;
    std::unordered_map<size_t, size_t> m_hashed_counts;

    static std::unique_ptr<Logger> s_instance;
    static std::mutex s_mutex;

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
        const size_t strhash = std::hash<std::string>{}(message);
        auto it = m_hashed_counts.find(strhash);

        if (it != m_hashed_counts.end()) {
            if (it->second <= 3) {
                m_messages.emplace_back(level, message);
                it->second++;
            }
            else if (it->second == 4) {
                m_messages.emplace_back(LogLevel::Info, "Silencing repeating message...");
                it->second++;
            }
        }
        else {
            m_hashed_counts.emplace(strhash, 1);
            m_messages.emplace_back(level, message);
        }
    };

    void set_log_level(int level)
    {
        std::unique_lock<std::mutex> lock(s_mutex);
        m_log_level = level;
    }

    int get_log_level()
    {
        std::unique_lock<std::mutex> lock(s_mutex);
        return m_log_level;
    }

    template <typename MessageHandlerFunc>
    void for_each_message(MessageHandlerFunc&& f)
    {
        std::unique_lock<std::mutex> lock(s_mutex);
        for (const LogMessage& message : m_messages) {
            f(message);
        }
    }

    inline size_t message_count()
    {
        std::unique_lock<std::mutex> lock(s_mutex);
        return m_messages.size();
    }

private:
    int m_log_level = (int)LogLevel::Debug;
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
        if (Logger::get_instance()->get_log_level() >= (int)level) {
            oss << std::endl;
            Logger::get_instance()->log(level, oss.str());
        }
    }
};

