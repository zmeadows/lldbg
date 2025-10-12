
#include "StreamBuffer.hpp"

#include "Log.hpp"

#include <array>

StreamBuffer::StreamBuffer(StreamSource source) : m_buf(), m_source(source)
{
    m_buf.reserve(2048);
}

void StreamBuffer::update(const lldb::SBProcess& process)
{
    std::array<char, 4096> tmp{};

    for (;;)
    {
        std::size_t n = 0;
        switch (m_source)
        {
        case StreamSource::StdOut:
            // keep your existing stdout read call here:
            n = process.GetSTDOUT(tmp.data(), tmp.size());
            break;
        case StreamSource::StdErr:
            // keep your existing stderr read call here:
            n = process.GetSTDERR(tmp.data(), tmp.size());
            break;
        }
        if (n == 0)
        {
            break;
        }

        const std::size_t room = (m_buf.size() < MAX_CAPACITY) ? (MAX_CAPACITY - m_buf.size()) : 0;

        if (n > room)
        {
            m_buf.append(tmp.data(), room);
            LOG(Warning) << "Exceeded maximum stream buffer capacity.";
            break;
        }

        m_buf.append(tmp.data(), n); // c_str() remains NUL-terminated
    }
}
