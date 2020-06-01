#include "LLDBEventListenerThread.hpp"

#include <assert.h>

#include <iostream>

#include "Log.hpp"
#include "lldb/API/LLDB.h"

namespace lldbg {

void LLDBEventListenerThread::start(lldb::SBProcess process)
{
    // TODO: log process ID and compare when 'stop' is called
    // TODO: check Process state
    if (m_thread) {
        LOG(Warning) << "Attempted to double-start the LLDBEventListenerThread";
        this->stop(process);
    }

    m_listener.Clear();
    m_listener = lldb::SBListener("lldbg_listener");

    const uint32_t listen_flags = lldb::SBProcess::eBroadcastBitStateChanged |
                                  lldb::SBProcess::eBroadcastBitSTDOUT |
                                  lldb::SBProcess::eBroadcastBitSTDERR;

    process.GetBroadcaster().AddListener(m_listener, listen_flags);

    m_continue.store(true);

    assert(!m_thread);
    m_thread.reset(new std::thread(&LLDBEventListenerThread::poll_events, this));

    LOG(Debug) << "Successfully launched LLDBEventListenerThread.";
}

void LLDBEventListenerThread::stop(lldb::SBProcess process)
{
    // TODO: check Process state
    if (!m_thread) {
        LOG(Warning) << "Attempted to double-stop the LLDBEventListenerThread";
        return;
    }

    m_continue.store(false);
    m_thread->join();
    m_thread.reset(nullptr);

    process.GetBroadcaster().RemoveListener(m_listener);

    m_listener.Clear();
    m_events.clear();

    LOG(Debug) << "Successfully stopped LLDBEventListenerThread.";
}

void LLDBEventListenerThread::poll_events()
{
    while (m_continue.load()) {
        lldb::SBEvent event;
        if (m_listener.WaitForEvent(1, event)) {
            if (!event.IsValid()) {
                // TODO: print event description using event.GetDescription(stream)
                LOG(Warning) << "Invalid LLDB event encountered, skipping...";
            }

            m_events.push(event);

            const lldb::StateType state = lldb::SBProcess::GetStateFromEvent(event);
            if (state == lldb::eStateCrashed || state == lldb::eStateDetached ||
                state == lldb::eStateExited) {
                m_continue.store(false);
                return;
            }
        }
    }
}

}  // namespace lldbg
