#pragma once

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include "lldb/API/LLDB.h"

// A simple thread-safe queue for LLDB events
class LLDBEventQueue {
    std::mutex m_mutex;
    std::deque<lldb::SBEvent> m_events;

public:
    std::optional<lldb::SBEvent> pop(void);
    void push(const lldb::SBEvent& new_event);
    void clear(void);
    size_t size(void);
};

// A pollable thread for collecting LLDB events from a queue
class LLDBEventListenerThread {
    lldb::SBListener m_listener;
    std::atomic<bool> m_continue;
    LLDBEventQueue m_events;
    std::unique_ptr<std::thread> m_thread;

    void poll_events();

public:
    LLDBEventListenerThread(lldb::SBDebugger&);
    ~LLDBEventListenerThread();

    std::optional<lldb::SBEvent> pop_event(void) { return m_events.pop(); }
    const lldb::SBListener& get_lldb_listener(void) const { return m_listener; };

    // TODO: write macros for these
    LLDBEventListenerThread() = delete;
    LLDBEventListenerThread(const LLDBEventListenerThread&) = delete;
    LLDBEventListenerThread(LLDBEventListenerThread&&) = delete;
    LLDBEventListenerThread& operator=(const LLDBEventListenerThread&) = delete;
    LLDBEventListenerThread& operator=(LLDBEventListenerThread&&) = delete;
};

