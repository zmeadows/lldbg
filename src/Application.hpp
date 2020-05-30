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

struct ExitDialog {
    std::string process_name;
    int exit_code;
};

// TODO: move some of the static variables from draw(Application) to this struct
// TODO: create sane Constructor and initialize UserInterface and Application *before* adding
// targets in main
// TODO: use std::optional<int> instead of -1
struct UserInterface {
    int viewed_thread_index = -1;
    int viewed_frame_index = -1;

    float window_width = -1.f;   // in pixels
    float window_height = -1.f;  // in pixels

    float file_browser_width;
    float file_viewer_width;
    float file_viewer_height;
    float console_height;

    bool request_manual_tab_change = false;
    bool ran_command_last_frame = false;
    bool window_resized_last_frame = false;

    ImFont* font = nullptr;
    GLFWwindow* window = nullptr;
};

struct Application {
    lldb::SBDebugger debugger;
    LLDBEventListenerThread event_listener;
    LLDBCommandLine command_line;
    OpenFiles open_files;
    BreakPointSet breakpoints;  // TODO: move to TextEditor
    std::unique_ptr<lldbg::FileBrowserNode> file_browser;
    UserInterface ui;
    TextEditor text_editor;
    std::optional<ExitDialog> exit_dialog;
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
