#pragma once

#include <cassert>
#include <iostream>

#include "FileSystem.hpp"
#include "LLDBCommandLine.hpp"
#include "LLDBEventListenerThread.hpp"
#include "Log.hpp"
#include "StreamBuffer.hpp"
#include "TextEditor.h"

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
    const int argc;
    const char** argv;

    DebugSession() = delete;

    // static std::optional<DebugSession> create(const std::string& exe_path, bool stop_at_entry);
};

// TODO: create sane Constructor and initialize UserInterface and Application *before* adding
// targets in main
// TODO: use std::optional<int> instead of -1
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
    std::optional<DebugSession> session;
    lldb::SBDebugger debugger;
    LLDBEventListenerThread event_listener;
    LLDBCommandLine command_line;
    OpenFiles open_files;
    BreakPointSet breakpoints;  // TODO: move to TextEditor
    std::unique_ptr<lldbg::FileBrowserNode> file_browser;
    UserInterface ui;
    TextEditor text_editor;
    StreamBuffer stdout_buf;
    StreamBuffer stderr_buf;
    char** argv;

    // TODO: make this a non-member function
    void main_loop(void);

    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application& operator=(Application&&) = delete;
};

void set_workdir(Application& app, const std::string& workdir);

enum class TargetAddResult {
    ExeDoesNotExistError,
    TargetCreateError,
    TooManyTargetsError,
    UnknownError,
    Success
};

TargetAddResult add_target(Application& app, const std::string& exe_path);

enum class TargetStartResult {
    NoTargetError,
    LaunchError,
    AttachTimeoutError,
    UnknownError,
    Success
};

TargetStartResult start_target(Application& app);

}  // namespace lldbg
