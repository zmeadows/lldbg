#include "DebugSession.hpp"

#include <cassert>

#include "Log.hpp"

DebugSession::DebugSession()
    : m_debugger(lldb::SBDebugger::Create()),
      m_cmdline(m_debugger),
      m_listener(m_debugger),
      m_stdout(StreamBuffer::StreamSource::StdOut),
      m_stderr(StreamBuffer::StreamSource::StdErr)
{
}

// TODO: if process running, present user with dialogue and give option to detach from lldb rather
// than kill process. But don't do that here, rather in a callback for application exit request.
DebugSession::~DebugSession(void)
{
    if (auto process = find_process(); process.has_value() && process->IsValid()) {
        LOG(Warning) << "Found active process while destructing DebugSession.";
        kill_process(*process);
    }

    if (auto target = find_target(); target.has_value() && target->IsValid()) {
        LOG(Warning) << "Found active target while destructing DebugSession.";
        m_debugger.DeleteTarget(*target);
    }

    if (m_debugger.IsValid()) {
        lldb::SBDebugger::Destroy(m_debugger);
        m_debugger.Clear();
    }
    else {
        LOG(Warning) << "Found invalid lldb::SBDebugger while destructing DebugSession.";
    }
}

std::optional<lldb::SBTarget> DebugSession::find_target()
{
    if (m_debugger.GetNumTargets() > 0) {
        auto target = m_debugger.GetSelectedTarget();
        if (!target.IsValid()) {
            LOG(Warning) << "Selected target is invalid";
            return {};
        }
        else {
            return target;
        }
    }
    else {
        return {};
    }
}

std::optional<lldb::SBProcess> DebugSession::find_process()
{
    auto target = find_target();

    if (!target.has_value()) return {};

    lldb::SBProcess process = target->GetProcess();

    if (!process.IsValid()) {
        return {};
    }

    return process;
}

void DebugSession::read_stream_buffers(void)
{
    // TODO: only do this upon receiving stdout/stderr process events
    if (auto process = find_process(); process.has_value()) {
        m_stdout.update(*process);
        m_stderr.update(*process);
    }
}

bool process_is_running(lldb::SBProcess& process)
{
    return process.IsValid() && process.GetState() == lldb::eStateRunning;
}

bool process_is_stopped(lldb::SBProcess& process)
{
    return process.IsValid() && process.GetState() == lldb::eStateStopped;
}

std::pair<bool, bool> process_is_finished(lldb::SBProcess& process)
{
    if (!process.IsValid()) {
        return {false, false};
    }

    const lldb::StateType state = process.GetState();

    const bool exited = state == lldb::eStateExited;
    const bool failed = state == lldb::eStateCrashed;

    return {exited || failed, !failed};
}

void stop_process(lldb::SBProcess& process)
{
    if (!process.IsValid()) {
        LOG(Warning) << "Attempted to stop an invalid process.";
        return;
    }

    if (process_is_stopped(process)) {
        LOG(Warning) << "Attempted to stop an already-stopped process.";
        return;
    }

    lldb::SBError err = process.Stop();
    if (err.Fail()) {
        LOG(Error) << "Failed to stop the process, encountered the following error: "
                   << err.GetCString();
        return;
    }
}

void continue_process(lldb::SBProcess& process)
{
    if (!process.IsValid()) {
        LOG(Warning) << "Attempted to continue an invalid process.";
        return;
    }

    if (process_is_running(process)) {
        LOG(Warning) << "Attempted to continue an already-running process.";
        return;
    }

    lldb::SBError err = process.Continue();
    if (err.Fail()) {
        LOG(Error) << "Failed to continue the process, encountered the following error: "
                   << err.GetCString();
        return;
    }
}

void kill_process(lldb::SBProcess& process)
{
    if (!process.IsValid()) {
        LOG(Warning) << "Attempted to kill an invalid process.";
        return;
    }

    if (process_is_finished(process).first) {
        LOG(Warning) << "Attempted to kill an already-finished process.";
        return;
    }

    lldb::SBError err = process.Kill();
    if (err.Fail()) {
        LOG(Error) << "Failed to kill the process, encountered the following error: "
                   << err.GetCString();
        return;
    }
}

lldb::SBCommandReturnObject DebugSession::run_lldb_command(const char* command,
                                                           bool hide_from_history)
{
    if (auto unaliased_cmd = m_cmdline.expand_and_unalias_command(command);
        unaliased_cmd.has_value()) {
        LOG(Debug) << "Unaliased command: " << *unaliased_cmd;
    }
    auto target_before = find_target();

    lldb::SBCommandReturnObject ret = m_cmdline.run_command(command, hide_from_history);

    auto target_after = find_target();

    // TODO: need to think a bit about handling multiple targets
    if ((!target_before && target_after) ||
        (target_before && target_after && (*target_before != *target_after))) {
        LOG(Debug) << "added listener to new target";
        target_after->GetBroadcaster().AddListener(
            m_listener.get_lldb_listener(), lldb::SBTarget::eBroadcastBitBreakpointChanged |
                                                lldb::SBTarget::eBroadcastBitWatchpointChanged);
    }

    switch (ret.GetStatus()) {
        case lldb::eReturnStatusInvalid:
            LOG(Debug) << "eReturnStatusInvalid";
            break;
        case lldb::eReturnStatusSuccessFinishNoResult:
            LOG(Debug) << "eReturnStatusSuccessFinishNoResult";
            break;
        case lldb::eReturnStatusSuccessFinishResult:
            LOG(Debug) << "eReturnStatusSuccessFinishResult";
            break;
        case lldb::eReturnStatusSuccessContinuingNoResult:
            LOG(Debug) << "eReturnStatusSuccessContinuingNoResult";
            break;
        case lldb::eReturnStatusSuccessContinuingResult:
            LOG(Debug) << "eReturnStatusSuccessContinuingResult";
            break;
        case lldb::eReturnStatusStarted:
            LOG(Debug) << "eReturnStatusStarted";
            break;
        case lldb::eReturnStatusFailed:
            LOG(Debug) << "eReturnStatusFailed";
            break;
        case lldb::eReturnStatusQuit:
            LOG(Debug) << "eReturnStatusQuit";
            break;
        default:
            LOG(Debug) << "unknown lldb command return status encountered.";
            break;
    }

    return ret;
}

DebugSession::State DebugSession::get_state(void)
{
    auto target = find_target();

    if (!target.has_value()) {
        return State::NoTarget;
    }

    auto process = find_process();

    if (!process.has_value()) {
        return State::ProcessNotYetLaunched;
    }

    if (process_is_stopped(*process)) {
        return State::ProcessStopped;
    }

    if (process_is_running(*process)) {
        return State::ProcessRunning;
    }

    if (const auto [finished, _] = process_is_finished(*process); finished) {
        return State::ProcessFinished;
    }

    LOG(Error) << "Unknown/Invalid DebugSession::SessionState encountered!";
    return State::Invalid;
}

const std::vector<CommandLineEntry>& DebugSession::get_lldb_command_history() const
{
    return m_cmdline.get_history();
}

std::optional<StopInfo> DebugSession::handle_lldb_events(void)
{
    std::optional<StopInfo> stop_info = {};

    while (true) {
        auto maybe_event = m_listener.pop_event();
        if (!maybe_event.has_value()) break;
        auto event = *maybe_event;

        if (!event.IsValid()) {
            LOG(Warning) << "Invalid event found.";
            continue;
        }

        auto target = find_target();
        auto process = find_process();

        lldb::SBStream event_description;
        event.GetDescription(event_description);

        if (target.has_value() && event.BroadcasterMatchesRef(target->GetBroadcaster())) {
            LOG(Debug) << "Found target event";
        }
        else if (process.has_value() && event.BroadcasterMatchesRef(process->GetBroadcaster())) {
            const lldb::StateType new_state = lldb::SBProcess::GetStateFromEvent(event);
            const char* state_descr = lldb::SBDebugger::StateAsCString(new_state);

            if (state_descr) {
                LOG(Debug) << "Found process event with new state: " << state_descr;
            }

            // For now we find the first (if any) stopped thread and construct a StopInfo.
            if (new_state == lldb::eStateStopped && process_is_stopped(*process)) {
                const uint32_t nthreads = process->GetNumThreads();
                for (uint32_t i = 0; i < nthreads; i++) {
                    lldb::SBThread th = process->GetThreadAtIndex(i);
                    switch (th.GetStopReason()) {
                        case lldb::eStopReasonBreakpoint: {
                            // https://lldb.llvm.org/cpp_reference/classlldb_1_1SBThread.html#af284261156e100f8d63704162f19ba76
                            assert(th.GetStopReasonDataCount() == 2);
                            lldb::break_id_t breakpoint_id = th.GetStopReasonDataAtIndex(0);
                            lldb::SBBreakpoint breakpoint =
                                target->FindBreakpointByID(breakpoint_id);

                            lldb::break_id_t location_id = th.GetStopReasonDataAtIndex(1);
                            lldb::SBBreakpointLocation location =
                                breakpoint.FindLocationByID(location_id);

                            // TODO: factor out, as this same sequence is also used in
                            // drawing the breakpoints within draw_breakpoints_and_watchpoints
                            lldb::SBAddress address = location.GetAddress();
                            lldb::SBLineEntry line_entry = address.GetLineEntry();
                            const char* filename = line_entry.GetFileSpec().GetFilename();
                            const char* directory = line_entry.GetFileSpec().GetDirectory();

                            fs::path breakpoint_filepath;
                            if (filename == nullptr || directory == nullptr) {
                                LOG(Error)
                                    << "Failed to read breakpoint location after thread halted";
                                continue;
                            }
                            else {
                                stop_info = {fs::path(directory) / fs::path(filename),
                                             line_entry.GetLine()};
                            }
                        }
                        default: {
                            continue;
                        }
                    }
                }
            }

            // TODO: if stop reason is breakpoint, identify file/line and pass back to
            // Application so that it may change viewer focus to that file/line.
            // USE GetStopReasonDataAtIndex(uint32_t idx)
        }
        else {
            // TODO: print event description
            LOG(Debug) << "Found non-target/process event";
        }

        LOG(Debug) << "    " << event_description.GetData();
    }

    return stop_info;
}

