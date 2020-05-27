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

    static constexpr float DEFAULT_FILEBROWSER_WIDTH_PERCENT = 0.2;
    static constexpr float DEFAULT_FILEVIEWER_WIDTH_PERCENT = 0.52f;
    static constexpr float DEFAULT_STACKTRACE_WIDTH_PERCENT = 0.28f;
};

// TODO: optimize this to handle both large amounts of stdout overall
// as well as very large rate of stdout messages
class StreamBuffer {
    size_t m_offset;
    size_t m_capacity;
    char* m_data;

    static const size_t MAX_CAPACITY = static_cast<size_t>(2e9);

public:
    void update(lldb::SBProcess process);

    inline const char* get(void) { return m_data; }

    StreamBuffer(void);
    ~StreamBuffer(void);

    StreamBuffer(const StreamBuffer&) = delete;
    StreamBuffer& operator=(const StreamBuffer&) = delete;
    StreamBuffer& operator=(StreamBuffer&&) = delete;
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
    StreamBuffer stdout_buf;

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

TargetStartResult start_target(Application& app, const char** argv);

}  // namespace lldbg
