#pragma once

#include <cassert>
#include <iostream>

#include "FileSystem.hpp"
#include "LLDBCommandLine.hpp"
#include "LLDBEventListenerThread.hpp"
#include "Log.hpp"
#include "TextEditor.h"
#include "lldb/API/LLDB.h"

// clang-format off
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl2.h"
// clang-format on

#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace lldbg {

struct ExitDialog {
    std::string process_name;
    int exit_code;
};

// TODO: rename UserInterface and move GLFWwindow/ImFont to Application
// TODO: can some of these fields be moved to static variables in the draw function?
struct RenderState {
    int viewed_thread_index = -1;
    int viewed_frame_index = -1;
    int window_width = -1;
    int window_height = -1;
    bool request_manual_tab_change = false;
    bool ran_command_last_frame = false;
    ImFont* font = nullptr;
    GLFWwindow* window = nullptr;

    static constexpr float DEFAULT_FILEBROWSER_WIDTH_PERCENT = 0.12f;
    static constexpr float DEFAULT_FILEVIEWER_WIDTH_PERCENT = 0.6f;
    static constexpr float DEFAULT_STACKTRACE_WIDTH_PERCENT = 0.28f;
};

struct Application {
    lldb::SBDebugger debugger;
    LLDBEventListenerThread event_listener;
    LLDBCommandLine command_line;
    OpenFiles open_files;
    BreakPointSet breakpoints;
    std::unique_ptr<lldbg::FileBrowserNode> file_browser;
    RenderState render_state;
    TextEditor text_editor;
    std::optional<ExitDialog> exit_dialog;

    // TODO: make this a non-member function
    void main_loop(void);

    Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application& operator=(Application&&) = delete;
    ~Application();
};

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

TargetStartResult start_target(Application& app, const char** argv);

}  // namespace lldbg
