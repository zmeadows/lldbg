#pragma once

#include <filesystem>
namespace fs = std::filesystem;

#include "EventListener.hpp"
#include "LLDBCommandLine.hpp"
#include "StreamBuffer.hpp"
#include "lldb/API/LLDB.h"

void stop_process(lldb::SBProcess&);
void continue_process(lldb::SBProcess&);
void kill_process(lldb::SBProcess&);
bool process_is_running(lldb::SBProcess&);
bool process_is_stopped(lldb::SBProcess&);

// TODO: refactor with struct instead of pair<bool,bool>
std::pair<bool, bool> process_is_finished(lldb::SBProcess&);

struct StopInfo {
    fs::path filepath;
    uint32_t line_number;
};

class DebugSession {
public:
    std::optional<lldb::SBTarget> find_target();
    std::optional<lldb::SBProcess> find_process();

    lldb::SBCommandReturnObject run_lldb_command(const char* command,
                                                 bool hide_from_history = false);

    const std::vector<CommandLineEntry>& get_lldb_command_history() const;

    // TODO: just write stdout/stderr directly to console upon stdout/stderr events from listener
    const StreamBuffer& stdout(void) { return m_stdout; }
    const StreamBuffer& stderr(void) { return m_stderr; }

    enum class State {
        Invalid,
        NoTarget,
        ProcessNotYetLaunched,
        ProcessStopped,
        ProcessRunning,
        ProcessFinished
    };

    State get_state(void);

    std::optional<StopInfo> handle_lldb_events(void);

    // TODO: combine console, log, stdout/stderr into just a single console
    void read_stream_buffers(void);

    DebugSession();
    ~DebugSession(void);

    DebugSession(DebugSession&&) = delete;
    DebugSession(const DebugSession&) = delete;
    DebugSession& operator=(const DebugSession&) = delete;
    DebugSession& operator=(DebugSession&&) = delete;

private:
    lldb::SBDebugger m_debugger;
    LLDBCommandLine m_cmdline;
    LLDBEventListenerThread m_listener;
    StreamBuffer m_stdout;
    StreamBuffer m_stderr;
};
