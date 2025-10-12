#pragma once

#include "fmt/core.h"
#include "fmt/format.h"

// Just a convenience class to ensure all fmt buffers are nul-terminated
class StringBuffer
{
    fmt::basic_memory_buffer<char> m_buffer;

  public:
    template <typename... Args> void format(const char* fmt_str, Args&&... args)
    {
        // TODO: loop over args parameters, check if they are pointers and if they
        // are null, don't call fmt::format_to
        fmt::format_to(std::back_inserter(m_buffer), fmt_str, std::forward<Args>(args)...);
        m_buffer.push_back('\0');
    }

    template <typename... Args> void format_(const char* fmt_str, Args&&... args)
    {
        // TODO: loop over args parameters, check if they are pointers and if they
        // are null, don't call fmt::format_to
        fmt::format_to(std::back_inserter(m_buffer), fmt_str, std::forward<Args>(args)...);
    }

    const char* data()
    {
        return m_buffer.data();
    }

    void clear()
    {
        m_buffer.clear();
    }

    void push_back(char chr)
    {
        m_buffer.push_back(chr);
    }
};
