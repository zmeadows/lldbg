#pragma once

#include "lldb/API/LLDB.h" // IWYU pragma: keep

#include <cstdint>
#include <string>
#include <string_view>

class StreamBuffer
{
  public:
    enum class StreamSource : std::uint8_t
    {
        StdOut,
        StdErr
    };

    explicit StreamBuffer(StreamSource source);

    void update(const lldb::SBProcess& process);

    void clear()
    {
        m_buf.clear();
    }
    [[nodiscard]] const char* get() const
    {
        return m_buf.c_str();
    }
    [[nodiscard]] std::string_view view() const
    {
        return m_buf;
    }
    [[nodiscard]] std::size_t size() const
    {
        return m_buf.size();
    }

  private:
    std::string m_buf;           // owns the bytes; always NUL-terminated via c_str()
    const StreamSource m_source; // TODO: maybe a template param later

    static constexpr std::size_t MAX_CAPACITY = static_cast<std::size_t>(2e9);
};
