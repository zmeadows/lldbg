#pragma once

#include "lldb/API/LLDB.h"

#include "FileSystem.hpp"
#include "LLDBCommandLine.hpp"
#include "LLDBEventListenerThread.hpp"
#include "Log.hpp"
#include "TextEditor.h"

#include <cassert>
#include <iostream>

// clang-format off
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl2.h"
// clang-format on

#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace lldbg {

// TODO: rename UserInterface and move GLFWwindow/ImFont to Application
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

struct ExitDialog {
    std::string process_name;
    int exit_code;
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

    void main_loop(void);

    Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application& operator=(Application&&) = delete;

    ~Application();
};

void delete_current_targets(Application& app);
void kill_process(Application& app);
void pause_process(Application& app);
void continue_process(Application& app);
void handle_event(Application& app, lldb::SBEvent);
void manually_open_and_or_focus_file(Application& app, const char* filepath);
bool run_lldb_command(Application& app, const char* command);
void add_breakpoint_to_viewed_file(Application& app, int line);

// We only use a global variable because of how freeglut
// requires a frame update function with signature void(void).
// It is not used anywhere other than main_loop from main.cpp
extern std::unique_ptr<Application> g_application;

struct TargetStartError {
    std::string msg = "Unknown error!";
    enum class Type {
        ExecutableDoesNotExist,
        TargetCreation,
        Launch,
        AttachTimeout,
        HasTargetAlready,
        Unknown
    } type = Type::Unknown;
};

// TODO: rename switch_to_target?
const std::optional<TargetStartError>
create_new_target(Application& app, const char* exe_filepath, const char** argv,
                  bool delay_start = true, std::optional<std::string> workdir = {});

// void reset(Application& app);

lldb::SBProcess get_process(Application& app);

} // namespace lldbg
