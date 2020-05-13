#include "LLDBEventListenerThread.hpp"

#include <assert.h>

#include <iostream>

#include "Log.hpp"
#include "lldb/API/LLDB.h"

namespace lldbg {

LLDBEventListenerThread::LLDBEventListenerThread() : m_listener(), m_continue(false) {}

void LLDBEventListenerThread::start(lldb::SBDebugger& debugger)
{
    m_listener = debugger.GetListener();

    const uint32_t listen_flags = lldb::SBProcess::eBroadcastBitStateChanged |
                                  lldb::SBProcess::eBroadcastBitSTDOUT |
                                  lldb::SBProcess::eBroadcastBitSTDERR;

    // TODO: when will we have to deal with multiple targets/processes?
    debugger.GetSelectedTarget().GetProcess().GetBroadcaster().AddListener(m_listener,
                                                                           listen_flags);

    m_continue.store(true);

    if (!m_thread) {
        m_thread.reset(new std::thread(&LLDBEventListenerThread::poll_events, this));
    }

    LOG(Debug) << "Successfully launched LLDBEventListenerThread.";
}

void LLDBEventListenerThread::stop(lldb::SBDebugger& debugger)
{
    m_continue.store(false);
    m_thread->join();
    m_thread.reset(nullptr);

    debugger.GetSelectedTarget().GetProcess().GetBroadcaster().RemoveListener(m_listener);

    m_listener.Clear();

    LOG(Debug) << "Successfully stopped LLDBEventListenerThread.";
}

void LLDBEventListenerThread::poll_events()
{
    while (m_continue.load()) {
        lldb::SBEvent event;
        // TODO: shorten wait time so that we can exit instantly when the user presses quit
        if (m_listener.WaitForEvent(1, event)) {
            assert(event.IsValid());
            m_events.push(event);
        }
    }
}

}  // namespace lldbg
