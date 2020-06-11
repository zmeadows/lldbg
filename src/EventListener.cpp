#include "EventListener.hpp"

#include <assert.h>

#include <iostream>

#include "Log.hpp"
#include "lldb/API/LLDB.h"

size_t LLDBEventQueue::size(void)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_events.size();
}

void LLDBEventQueue::push(const lldb::SBEvent& new_event)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_events.push_back(new_event);
}

std::optional<lldb::SBEvent> LLDBEventQueue::pop(void)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_events.empty()) {
        return {};
    }
    else {
        auto event = m_events.front();
        m_events.pop_front();
        return event;
    }
}

void LLDBEventQueue::clear(void)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_events.clear();
}

LLDBEventListenerThread::LLDBEventListenerThread(lldb::SBDebugger& debugger)
    : m_listener(debugger.GetListener()),
      m_continue(true),
      m_events(),
      m_thread(new std::thread(&LLDBEventListenerThread::poll_events, this))
{
}

LLDBEventListenerThread::~LLDBEventListenerThread()
{
    m_continue.store(false);
    m_thread->join();
    m_thread.reset(nullptr);
    m_events.clear();
    LOG(Debug) << "Successfully stopped LLDBEventListenerThread.";
}

void LLDBEventListenerThread::poll_events()
{
    LOG(Debug) << "Launched LLDBEventListenerThread.";

    while (m_continue.load()) {
        lldb::SBEvent event;
        if (m_listener.WaitForEvent(1, event)) {
            if (!event.IsValid()) {
                LOG(Warning) << "Invalid LLDB event encountered, skipping...";
                continue;
            }
            // TODO: log event description using event.GetDescription(stream)

            m_events.push(event);

            // In the case where the process has crashed/exited, we stop the thread immediately to
            // avoid any more WaitForEvent delays in the case of program shutdown
            // TODO: use global std::atomic<bool> to check for program shutdown
            // if (lldb::SBProcess::EventIsProcessEvent(event)) {
            //     const lldb::StateType state = lldb::SBProcess::GetStateFromEvent(event);
            //     if (state == lldb::eStateCrashed || state == lldb::eStateDetached ||
            //         state == lldb::eStateExited) {
            //         m_continue.store(false);
            //         return;
            //     }
            // }
        }
    }
}
