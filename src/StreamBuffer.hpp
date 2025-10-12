#pragma once

#include "lldb/API/LLDB.h"

class StreamBuffer
{
  public:
    enum class StreamSource
    {
        StdOut,
        StdErr
    };

    void update(lldb::SBProcess process);
    inline const char* get(void) const
    {
        return m_data;
    }
    void clear(void);
    inline size_t size(void) const
    {
        return m_offset;
    }

    StreamBuffer(StreamSource source);
    ~StreamBuffer(void);

    StreamBuffer(const StreamBuffer&) = delete;
    StreamBuffer& operator=(const StreamBuffer&) = delete;
    StreamBuffer& operator=(StreamBuffer&&) = delete;

  private:
    size_t m_offset;
    size_t m_capacity;
    char* m_data;
    const StreamSource m_source; // TODO: switch this to template parameter

    static const size_t MAX_CAPACITY = static_cast<size_t>(2e9);
};
