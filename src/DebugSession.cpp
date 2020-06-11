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

// https://lists.llvm.org/pipermail/lldb-dev/2016-January/009454.html
// std::unique_ptr<DebugSession>
// DebugSession::create(const std::string& exe_path,
//                                                    const std::vector<std::string>& exe_args,
//                                                    uint32_t launch_flags)
// {
//     const fs::path full_exe_path = fs::canonical(exe_path);
//
//     if (!fs::exists(full_exe_path)) {  // TODO: check if regular file and executable, etc
//         LOG(Error) << "Requested executable does not exist: " << full_exe_path;
//         return nullptr;
//     }
//
//     lldb::SBDebugger debugger = lldb::SBDebugger::Create();
//     debugger.SetUseColor(false);
//     debugger.SetAsync(true);
//
//     lldb::SBError lldb_error;
//     lldb::SBTarget target =
//         debugger.CreateTarget(full_exe_path.c_str(), nullptr, nullptr, true, lldb_error);
//
//     if (lldb_error.Success() && target.IsValid() && debugger.IsValid()) {
//         LOG(Debug) << "Succesfully created/selected target for executable: " << full_exe_path;
//         lldb_error.Clear();
//     }
//     else {
//         auto lldb_error_cstr = lldb_error.GetCString();
//         LOG(Error) << (lldb_error_cstr ? lldb_error_cstr : "Unknown target creation error.");
//         lldb::SBDebugger::Destroy(debugger);
//         return nullptr;
//     }
//
//     return std::unique_ptr<DebugSession>(
//         new DebugSession(debugger, full_exe_path, exe_args, launch_flags));
// }

// bool DebugSession::start_process(std::optional<uint32_t> new_launch_flags)
// {
//     if (get_process().has_value()) {
//         LOG(Warning) << "Process already started.";
//         return false;
//     }
//
//     lldb::SBTarget target = get_target();
//
//     if (!target.IsValid()) {
//         LOG(Error) << "Invalid target encountered in DebugSession::start";
//         return false;
//     }
//
//     std::vector<const char*> argv_cstr;
//     argv_cstr.reserve(m_argv.size());
//     for (const auto& arg : m_argv) {
//         argv_cstr.push_back(arg.c_str());
//     }
//
//     lldb::SBLaunchInfo launch_info(argv_cstr.data());
//     assert(launch_info.GetNumArguments() == argv_cstr.size());
//
//     if (new_launch_flags.has_value()) {
//         m_launch_flags = *new_launch_flags;
//     }
//     // launch_info.SetLaunchFlags(lldb::eLaunchFlagStopAtEntry);
//     launch_info.SetLaunchFlags(m_launch_flags);
//
//     lldb::SBError lldb_error;
//     lldb::SBProcess process = target.Launch(launch_info, lldb_error);
//
//     if (lldb_error.Success() && process.IsValid()) {
//         LOG(Debug) << "Succesfully launched target process for executable: " << m_exe_path;
//         lldb_error.Clear();
//     }
//     else {
//         const char* lldb_error_cstr = lldb_error.GetCString();
//         LOG(Error) << (lldb_error_cstr ? lldb_error_cstr : "Unknown target launch error!");
//         return false;
//     }
//
//     size_t ms_attaching = 0;
//     while (process.GetState() == lldb::eStateAttaching) {
//         LOG(Debug) << "Attaching to new process...";
//         std::this_thread::sleep_for(std::chrono::milliseconds(100));
//         ms_attaching += 100;
//         if (ms_attaching / 1000 > 5) {
//             LOG(Error) << "Attach timeout after launching target process.";
//             return false;
//         }
//     }
//
//     LOG(Debug) << "Succesfully attached to target process for executable: " << m_exe_path;
//
//     std::cout << "process state: " << lldb::SBDebugger::StateAsCString(process.GetState())
//               << std::endl;
//
//     static char input_buf[2048];
//     for (uint32_t i = 0; i < process.GetNumThreads(); i++) {
//         auto thread = process.GetThreadAtIndex(i);
//         std::cout << "thread " << i << " StopReason: " << thread.GetStopReason() << std::endl;
//
//         thread.GetStopDescription(input_buf, 2048);
//         std::cout << "thread " << i << " stop description: " << input_buf << std::endl;
//     }
//
//     m_listener.start(process);
//     return true;
// }

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

    // TODO: remove target_before broadcast listening, in second case?
    if ((!target_before && target_after) ||
        (target_before && target_after && (*target_before != *target_after))) {
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
            LOG(Debug) << "unknown lldd command return status encountered.";
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

void DebugSession::handle_lldb_process_event(lldb::SBEvent& event)
{
    const lldb::StateType new_state = lldb::SBProcess::GetStateFromEvent(event);
    const char* state_descr = lldb::SBDebugger::StateAsCString(new_state);
    if (state_descr) {
        LOG(Debug) << "Found process event with new state: " << state_descr;
    }
}

void DebugSession::handle_lldb_target_event(lldb::SBEvent&) { LOG(Debug) << "Found target event"; }

void DebugSession::handle_lldb_events(void)
{
    while (true) {
        auto maybe_event = m_listener.pop_event();
        if (!maybe_event.has_value()) break;
        auto event = *maybe_event;

        if (!event.IsValid()) {
            LOG(Warning) << "Invalid event found.";
            continue;
        }

        auto process = find_process();
        auto target = find_target();

        if (target.has_value() && event.BroadcasterMatchesRef(target->GetBroadcaster())) {
            handle_lldb_target_event(event);
        }
        else if (process.has_value() && event.BroadcasterMatchesRef(process->GetBroadcaster())) {
            handle_lldb_process_event(event);
        }
        else {
            // TODO: print event description
            LOG(Debug) << "Found non-target/process event";
        }

        // if (lldb::SBProcess::EventIsProcessEvent(event)) {
        //     handle_lldb_process_event(event);
        // }
        // else if (lldb::SBTarget::EventIsTargetEvent(event)) {
        //     handle_lldb_target_event(event);
        // }
        // else {
        //     // TODO: print event description
        //     LOG(Debug) << "Found non-target/process event";
        // }

        // if (new_state == lldb::eStateCrashed || new_state == lldb::eStateDetached ||
        //     new_state == lldb::eStateExited) {
        //     auto process = find_process();
        //     if (process.has_value()) {
        //         m_listener.stop(*process);
        //     }
        //     else {
        //         LOG(Warning) << "Tried handling lldb event while process doesn't exist!";
        //     }
        // }
    }
}

