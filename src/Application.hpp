#pragma once

#include <lldb/API/LLDB.h>

#include <cassert>
#include <iostream>

#include "FPSTimer.hpp"
#include "FileSystem.hpp"
#include "FileViewer.hpp"
#include "LLDBCommandLine.hpp"
#include "LLDBEventListenerThread.hpp"
#include "Log.hpp"
#include "StreamBuffer.hpp"

// clang-format off
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl2.h"
// clang-format on

#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace lldbg {

class DebugSession {
public:
    static std::unique_ptr<DebugSession> create(const std::string& exe_path,
                                                const std::vector<std::string>& exe_args,
                                                uint32_t launch_flags);

    lldb::SBTarget get_target()
    {
        assert(m_debugger.GetNumTargets() != 0 && "No targets available");
        assert(m_debugger.GetNumTargets() > 1 &&
               "More than one target found (not currently supported)");
        return m_debugger.GetTargetAtIndex(0);
    }

    std::optional<lldb::SBProcess> get_process()
    {
        lldb::SBProcess process = get_target().GetProcess();

        if (!process.IsValid()) {
            LOG(Debug) << "Found invalid process. Is it waiting to be launched?";
            return {};
        }

        if (process.GetNumThreads() == 0) {
            LOG(Debug) << "Found valid process with zero threads. Is it waiting to be launched?";
            return {};
        }

        return process;
    }

    std::vector<lldb::SBThread> get_process_threads()
    {
        auto process = get_process();
        if (!process.has_value()) return {};

        const auto nthreads = process->GetNumThreads();
        std::vector<lldb::SBThread> result;
        result.reserve(nthreads);

        for (uint32_t i = 0; i < nthreads; i++) {
            result.push_back(process->GetThreadAtIndex(i));
        }

        return result;
    }

    void read_stream_buffers(void)
    {
        if (auto process = get_process(); process.has_value()) {
            m_stdout.update(*process);
            m_stderr.update(*process);
        }
    }

    bool process_is_running(void)
    {
        auto process = get_process();
        return process.has_value() && process->GetState() == lldb::eStateRunning;
    }

    bool process_is_stopped(void)
    {
        auto process = get_process();
        return process.has_value() && process->GetState() == lldb::eStateStopped;
    }

    bool process_is_finished(void)
    {
        auto process = get_process();
        if (!process.has_value()) return false;

        auto state = process->GetState();
        return state == lldb::eStateCrashed || state == lldb::eStateDetached ||
               state == lldb::eStateExited;
    }

    bool process_exited_successfully(void)
    {
        auto process = get_process();
        if (!process.has_value()) return false;

        return process->GetState() == lldb::eStateExited;
    }

    size_t num_breakpoints(void) { return get_target().GetNumBreakpoints(); }

    void start_process(std::optional<uint32_t> new_launch_flags);

    void stop_process(void)
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

    void continue_process(void)
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

    void kill_process(void)
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

    void run_lldb_command(const char* command, bool hide_from_history = false)
    {
        return m_cmdline.run_command(command, hide_from_history);
    }

    const std::vector<CommandLineEntry>& get_lldb_command_history() const
    {
        return m_cmdline.get_history();
    }

    void handle_lldb_events(void)
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

    const StreamBuffer& stdout(void) { return m_stdout; }
    const StreamBuffer& stderr(void) { return m_stderr; }

private:
    lldb::SBDebugger m_debugger;
    LLDBCommandLine m_cmdline;
    LLDBEventListenerThread m_listener;
    StreamBuffer m_stdout;
    StreamBuffer m_stderr;
    std::vector<std::string> m_argv;
    uint32_t m_launch_flags;

    DebugSession(lldb::SBDebugger&& debugger, const std::vector<std::string>& argv)
        : m_debugger(std::move(debugger)),
          m_cmdline(m_debugger),
          m_listener(),
          m_stdout(StreamBuffer::StreamSource::StdOut),
          m_stderr(StreamBuffer::StreamSource::StdErr),
          m_argv(argv)
    {
    }

    ~DebugSession(void) { lldb::SBDebugger::Destroy(m_debugger); }

    DebugSession() = delete;
    DebugSession(DebugSession&&) = delete;
    DebugSession(const DebugSession&) = delete;
    DebugSession& operator=(const DebugSession&) = delete;
    DebugSession& operator=(DebugSession&&) = delete;
};

// TODO: create sane Constructor and initialize UserInterface and Application *before* adding
// targets in main
// TODO: use std::optional<size_t> instead of -1
// TODO: add size_t to keep track of frames_rendered
struct UserInterface {
    int viewed_thread_index = -1;
    int viewed_frame_index = -1;

    float window_width = -1.f;   // in pixels
    float window_height = -1.f;  // in pixels

    float file_browser_width = -1.f;
    float file_viewer_width = -1.f;
    float file_viewer_height = -1.f;
    float console_height = -1.f;

    bool request_manual_tab_change = false;
    bool ran_command_last_frame = false;
    bool window_resized_last_frame = false;

    ImFont* font = nullptr;
    GLFWwindow* window = nullptr;
};

// TODO: convert this struct to OOP-style
struct Application {
    std::unique_ptr<DebugSession> session;
    OpenFiles open_files;
    BreakPointSet breakpoints;  // TODO: move to TextEditor
    std::unique_ptr<lldbg::FileBrowserNode> file_browser;
    UserInterface ui;
    FileViewer text_editor;
    FPSTimer fps_timer;

    int main_loop(void);

    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application& operator=(Application&&) = delete;
};

void set_workdir(Application& app, const std::string& workdir);

}  // namespace lldbg
