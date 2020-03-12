#pragma once

#include "lldb/API/LLDB.h"

#include "FileSystem.hpp"
#include "Log.hpp"
#include "TextEditor.h"

#include "LLDBCommandLine.hpp"
#include "LLDBEventListenerThread.hpp"

#include <assert.h>
#include <iostream>
#include "imgui.h"

namespace lldbg {

// rename UserInterface?
struct RenderState {
    int viewed_thread_index = -1;
    int viewed_frame_index = -1;
    int window_width = -1;
    int window_height = -1;
    bool request_manual_tab_change = false;
    bool ran_command_last_frame = false;
    ImFont* font = nullptr;

    static constexpr float DEFAULT_FILEBROWSER_WIDTH_PERCENT = 0.12;
    static constexpr float DEFAULT_FILEVIEWER_WIDTH_PERCENT = 0.6;
    static constexpr float DEFAULT_STACKTRACE_WIDTH_PERCENT = 0.28;

    // TODO: add reset method and call when destroy/reset debug target
};

struct ExitDialog {
    std::string process_name;
    int exit_code;
};

struct Application {
    lldb::SBDebugger debugger;
    lldbg::LLDBEventListenerThread event_listener;
    lldbg::LLDBCommandLine command_line;
    lldbg::OpenFiles open_files;
    lldbg::BreakPointSet breakpoints;
    std::unique_ptr<lldbg::FileBrowserNode> file_browser;
    RenderState render_state;
    TextEditor text_editor;

    std::optional<ExitDialog> exit_dialog;

    Application(int* argcp, char** argv);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application& operator=(Application&&) = delete;
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

const std::optional<TargetStartError> create_new_target(Application& app, const char* exe_filepath,
                                                        const char** argv, bool delay_start = true,
                                                        std::optional<std::string> workdir = {});

// void reset(Application& app);

lldb::SBProcess get_process(Application& app);

}  // namespace lldbg
