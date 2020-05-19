#include "LLDBEventListenerThread.hpp"

#include <assert.h>

#include <iostream>

#include "Log.hpp"
#include "lldb/API/LLDB.h"

namespace lldbg {

void LLDBEventListenerThread::start(lldb::SBDebugger& debugger)
{
    // TODO: if no targets exist, simply log and return
    m_listener = debugger.GetListener();

    const uint32_t listen_flags = lldb::SBProcess::eBroadcastBitStateChanged |
                                  lldb::SBProcess::eBroadcastBitSTDOUT |
                                  lldb::SBProcess::eBroadcastBitSTDERR;

    debugger.GetSelectedTarget().GetProcess().GetBroadcaster().AddListener(m_listener,
                                                                           listen_flags);

    m_continue.store(true);

    // TODO: check earlier if thread is already running, if so log error and return
    if (!m_thread) {
        m_thread.reset(new std::thread(&LLDBEventListenerThread::poll_events, this));
    }

    LOG(Debug) << "Successfully launched LLDBEventListenerThread.";
}

void LLDBEventListenerThread::stop(lldb::SBDebugger& debugger)
{
    // TODO: check if already started and if not: log error and return
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
        // TODO: we can maybe even just move this into the main loop to avoid spawning a
        // dedicated thread
        if (m_listener.WaitForEvent(1, event)) {
            assert(event.IsValid());
            m_events.push(event);
        }
    }
}

}  // namespace lldbg
