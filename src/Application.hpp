#pragma once

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

#include "lldb/API/LLDB.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace lldbg {

struct DebugSession {
    lldb::SBTarget target;
    lldb::SBProcess process;
    LLDBEventListenerThread listener;
    StreamBuffer stdout_buf;
    StreamBuffer stderr_buf;
    int argc;
    char** argv;

private:
    DebugSession(lldb::SBTarget _target, int _argc, char** _argv)
        : target(_target),
          process(_target.GetProcess()),
          listener(),
          stdout_buf(StreamBuffer::StreamSource::StdOut),
          stderr_buf(StreamBuffer::StreamSource::StdErr),
          argc(_argc),
          argv(_argv)
    {
        assert(target.IsValid());
        assert(process.IsValid());
        this->listener.start(this->process);
    }

public:
    DebugSession() = delete;

    static std::unique_ptr<DebugSession> create(lldb::SBDebugger debugger,
                                                const std::string& exe_path, int argc, char** argv,
                                                bool stop_at_entry);
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

struct Application {
    std::unique_ptr<DebugSession> session;
    lldb::SBDebugger debugger;
    LLDBCommandLine command_line;
    OpenFiles open_files;
    BreakPointSet breakpoints;  // TODO: move to TextEditor
    std::unique_ptr<lldbg::FileBrowserNode> file_browser;
    UserInterface ui;
    FileViewer text_editor;
    FPSTimer fps_timer;

    // TODO: make this a non-member function
    void main_loop(void);

    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application& operator=(Application&&) = delete;
};

void set_workdir(Application& app, const std::string& workdir);

}  // namespace lldbg
