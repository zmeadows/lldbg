#pragma once

#include "fmt/format.h"

// Just a convenience class to ensure all fmt buffers are nul-terminated
class StringBuffer {
    fmt::memory_buffer m_buffer;

public:
    template <typename... Args>
    inline void format(const char* fmt_str, Args&&... args)
    {
        // TODO: loop over args parameters, check if they are pointers and if they are null, don't
        // call fmt::format_to
        fmt::format_to(m_buffer, fmt_str, args...);
        m_buffer.push_back('\0');
    }

    template <typename... Args>
    inline void format_(const char* fmt_str, Args&&... args)
    {
        // TODO: loop over args parameters, check if they are pointers and if they are null, don't
        // call fmt::format_to
        fmt::format_to(m_buffer, fmt_str, args...);
    }

    inline const char* data(void) { return m_buffer.data(); }

    inline void clear(void) { m_buffer.clear(); }
};
