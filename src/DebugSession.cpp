#include "DebugSession.hpp"

DebugSession::DebugSession()
    : m_debugger(lldb::SBDebugger::Create()),
      m_cmdline(m_debugger),
      m_listener(),
      m_stdout(StreamBuffer::StreamSource::StdOut),
      m_stderr(StreamBuffer::StreamSource::StdErr)
{
}

DebugSession::~DebugSession(void)
{
    kill_process();
    auto target = get_target();
    if (target.IsValid()) m_debugger.DeleteTarget(target);
    lldb::SBDebugger::Destroy(m_debugger);
}

// TODO: separate target/process listener threads :
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
    auto target = get_target();

    if (!target.has_value()) return {};

    lldb::SBProcess process = target->GetProcess();

    if (!process.IsValid()) {
        LOG(Debug) << "Found invalid process.";
        return {};
    }

    if (process.GetNumThreads() == 0) {
        LOG(Debug) << "Found valid process with zero threads.";
        return {};
    }

    return process;
}

// std::vector<lldb::SBThread> DebugSession::get_process_threads()
// {
//     auto process = get_process();
//     if (!process.has_value()) return {};
//
//     const auto nthreads = process->GetNumThreads();
//     std::vector<lldb::SBThread> result;
//     result.reserve(nthreads);
//
//     for (uint32_t i = 0; i < nthreads; i++) {
//         result.push_back(process->GetThreadAtIndex(i));
//     }
//
//     return result;
// }

void DebugSession::read_stream_buffers(void)
{
    if (auto process = get_process(); process.has_value()) {
        m_stdout.update(*process);
        m_stderr.update(*process);
    }
}

bool DebugSession::process_is_running(void)
{
    auto process = get_process();
    return process.has_value() && process->GetState() == lldb::eStateRunning;
}

bool DebugSession::process_is_stopped(void)
{
    auto process = get_process();
    return process.has_value() && process->GetState() == lldb::eStateStopped;
}

bool DebugSession::process_is_finished(void)
{
    auto process = get_process();
    if (!process.has_value()) return false;

    auto state = process->GetState();
    return state == lldb::eStateCrashed || state == lldb::eStateDetached ||
           state == lldb::eStateExited;
}

bool DebugSession::process_exited_successfully(void)
{
    auto process = get_process();
    if (!process.has_value()) return false;

    return process->GetState() == lldb::eStateExited;
}

void DebugSession::stop_process(void)
{
    auto process = get_process();

    if (!process.has_value()) {
        LOG(Warning) << "Attempted to stop a non-existent process.";
        return;
    }

    if (process_is_stopped()) {
        LOG(Warning) << "Attempted to stop an already-stopped process.";
        return;
    }

    process->Stop();
}

void DebugSession::continue_process(void)
{
    auto process = get_process();

    if (!process.has_value()) {
        LOG(Warning) << "Attempted to continue a non-existent process.";
        return;
    }

    if (process_is_running()) {
        LOG(Warning) << "Attempted to continue an already-running process.";
        return;
    }

    process->Continue();
}

void DebugSession::kill_process(void)
{
    auto process = get_process();

    if (!process.has_value()) {
        LOG(Warning) << "Attempted to kill a non-existent process.";
        return;
    }

    if (process_is_finished()) {
        LOG(Warning) << "Attempted to kill an already-finished process.";
        return;
    }

    process->Kill();
}

void DebugSession::run_lldb_command(const char* command, bool hide_from_history)
{
    const bool process_existed_before = get_process().has_value();
    m_cmdline.run_command(command, hide_from_history);
    const bool process_exists_after = get_process().has_value();
}

const std::vector<CommandLineEntry>& DebugSession::get_lldb_command_history() const
{
    return m_cmdline.get_history();
}

void DebugSession::handle_lldb_events(void)
{
    while (true) {
        lldb::SBEvent event;
        const bool found_event = m_listener.pop_event(event);
        if (!found_event) break;

        const lldb::StateType new_state = lldb::SBProcess::GetStateFromEvent(event);
        const char* state_descr = lldb::SBDebugger::StateAsCString(new_state);
        LOG(Debug) << "Found event with new state: " << state_descr;

        if (new_state == lldb::eStateCrashed || new_state == lldb::eStateDetached ||
            new_state == lldb::eStateExited) {
            auto process = get_process();
            if (process.has_value()) {
                m_listener.stop(*process);
            }
            else {
                LOG(Warning) << "Tried handling lldb event while process doesn't exist!";
            }
        }
    }
}
