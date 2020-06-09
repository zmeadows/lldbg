#pragma once

bool process_is_running(const lldb::SBProcess&);
bool process_is_stopped(const lldb::SBProcess&);
bool process_is_finished(const lldb::SBProcess&);
bool process_exited_successfully(const lldb::SBProcess&);

void stop_process(lldb::SBProcess&);
void continue_process(lldb::SBProcess&);
void kill_process(lldb::SBProcess&);

class DebugSession {
public:
    std::optional<lldb::SBTarget> find_target();
    std::optional<lldb::SBProcess> find_process();

    bool start_process(std::optional<uint32_t> new_launch_flags = {});

    void run_lldb_command(const char* command, bool hide_from_history = false);

    const std::vector<CommandLineEntry>& get_lldb_command_history() const;

    const StreamBuffer& stdout(void) { return m_stdout; }
    const StreamBuffer& stderr(void) { return m_stderr; }

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

    void handle_lldb_events(void);

    // TODO: combine console, log, stdout/stderr into just a single console
    void read_stream_buffers(void);
};
