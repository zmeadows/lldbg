#include "StreamBuffer.hpp"

#include "Log.hpp"

#include <cassert>

void StreamBuffer::update(lldb::SBProcess process)
{
    while (m_capacity < StreamBuffer::MAX_CAPACITY)
    {
        size_t bytes_written = 0;

        switch (m_source)
        {
        case (StreamSource::StdOut):
        {
            bytes_written = process.GetSTDOUT(m_data + m_offset, m_capacity - m_offset);
            break;
        }
        case (StreamSource::StdErr):
        {
            bytes_written = process.GetSTDERR(m_data + m_offset, m_capacity - m_offset);
            break;
        }
        }

        if (bytes_written == 0)
            return;

        m_offset += bytes_written;
        LOG(Verbose) << "read " << bytes_written
                     << " bytes from stream for new offset: " << m_offset;

        if (m_offset < m_capacity)
        { // success!
            return;
        }
        else
        {
            const size_t new_capacity = m_capacity * 2;
            assert(new_capacity > m_offset);

            LOG(Verbose) << "Reallocating stream buffer for larger capacity: " << m_capacity
                         << " bytes -> " << new_capacity << " bytes";

            m_capacity = new_capacity;

            char* new_data = (char*) malloc(sizeof(char) * m_capacity);
            assert(new_data != nullptr);

            for (size_t i = 0; i < m_offset; i++)
            {
                new_data[i] = m_data[i];
            }
            for (size_t i = m_offset; i < m_capacity; i++)
            {
                new_data[i] = '\0';
            }

            free(m_data);
            m_data = new_data;
        }
    }

    LOG(Warning) << "Exceeded maximum stream buffer capacity.";
}

void StreamBuffer::clear(void)
{
    for (size_t i = 0; i < m_capacity; i++)
    {
        m_data[i] = '\0';
    }
    m_offset = 0;
}

StreamBuffer::StreamBuffer(StreamSource source)
    : m_offset(0), m_capacity(2048), m_data((char*) malloc(sizeof(char) * m_capacity)),
      m_source(source)
{
    this->clear();
}

StreamBuffer::~StreamBuffer(void)
{
    if (m_data != nullptr)
    {
        free(m_data);
        m_data = nullptr;
    }
    m_capacity = 0;
    m_offset = 0;
}
