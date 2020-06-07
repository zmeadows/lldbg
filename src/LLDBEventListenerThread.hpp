#pragma once

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

#include "Log.hpp"
#include "lldb/API/LLDB.h"

namespace lldbg {

// A pollable thread for collecting LLDB events from a queue
class LLDBEventListenerThread {
    lldb::SBListener m_listener;
    std::unique_ptr<std::thread> m_thread;
    std::atomic<bool> m_continue;

    // A simple thread-safe queue for LLDB events
    class EventQueue {
        std::mutex m_mutex;
        std::deque<lldb::SBEvent> m_events;

    public:
        size_t size(void)
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            return m_events.size();
        }

        void push(const lldb::SBEvent& new_event)
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_events.push_back(new_event);
        }

        bool pop(lldb::SBEvent& event)
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            if (m_events.empty()) {
                return false;
            }

            event = m_events.front();
            m_events.pop_front();
            return true;
        }

        void clear(void)
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_events.clear();
        }
    };

    EventQueue m_events;

    void poll_events();

public:
    void start(lldb::SBProcess);
    void stop(lldb::SBProcess);

    // TODO: use std::optional
    bool pop_event(lldb::SBEvent& event) { return m_events.pop(event); }

    LLDBEventListenerThread() : m_continue(false) {}

    LLDBEventListenerThread(const LLDBEventListenerThread&) = delete;
    LLDBEventListenerThread& operator=(const LLDBEventListenerThread&) = delete;
    LLDBEventListenerThread& operator=(LLDBEventListenerThread&&) = delete;
    ~LLDBEventListenerThread()
    {
        if (m_thread) {
            LOG(Warning) << "Manually killing LLDBEventListenerThread, should not happen!";
            m_continue.store(false);
            m_thread->join();
            m_thread.reset(nullptr);
        }
    }
};

}  // namespace lldbg
