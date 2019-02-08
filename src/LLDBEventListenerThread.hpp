#pragma once

#include <deque>
#include <memory>
#include <mutex>
#include <thread>

#include "lldb/API/LLDB.h"

namespace {

class EventQueue {
    std::mutex m_mutex;
    std::deque<lldb::SBEvent> m_events;

public:
    inline size_t size(void) {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_events.size();
    }

    inline void push(const lldb::SBEvent& new_event) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_events.push_back(new_event);
    }

    inline lldb::SBEvent pop(void) {
        std::unique_lock<std::mutex> lock(m_mutex);
        lldb::SBEvent front_copy = m_events.front();
        m_events.pop_front();
        return front_copy;
    }

    inline void clear(void) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_events.clear();
    }
};

}

namespace lldbg {

class LLDBEventListenerThread {
    lldb::SBListener m_listener;
    std::unique_ptr<std::thread> m_thread;
    std::atomic<bool> m_continue;
    EventQueue m_events;

    void poll_events();

public:
    void start(lldb::SBDebugger&);
    void stop(lldb::SBDebugger&);

    //TODO: what if an event is consumed between these calls?
    //Doesn't matter if we only have one consumer
    inline bool events_are_waiting() { return m_events.size() > 0; }
    inline lldb::SBEvent pop_event() { return m_events.pop(); }

    LLDBEventListenerThread();

    LLDBEventListenerThread(const LLDBEventListenerThread&) = delete;
    LLDBEventListenerThread& operator=(const LLDBEventListenerThread&) = delete;
    LLDBEventListenerThread& operator=(LLDBEventListenerThread&&) = delete;
};

}
