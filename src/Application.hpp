#pragma once

#include "lldb/API/LLDB.h"

#include "Log.hpp"
#include "FileSystem.hpp"
#include "TextEditor.h"

#include "LLDBEventListenerThread.hpp"
#include "LLDBCommandLine.hpp"

#include "imgui.h"
#include <iostream>
#include <assert.h>

namespace lldbg {

using err_t = int;

struct RenderState final {
    int viewed_thread_index = -1;
    int viewed_frame_index = -1;
    int window_width = -1;
    int window_height = -1;
    bool request_manual_tab_change = false;
    bool ran_command_last_frame = false;
    ImFont* font = nullptr;

    static constexpr float DEFAULT_FILEBROWSER_WIDTH_PERCENT = 0.2;
    static constexpr float DEFAULT_FILEVIEWER_WIDTH_PERCENT = 0.6;
    static constexpr float DEFAULT_STACKTRACE_WIDTH_PERCENT = 0.2;
};


struct Application final {
    lldb::SBDebugger debugger;
    lldbg::LLDBEventListenerThread event_listener;
    lldbg::LLDBCommandLine command_line;
    lldbg::FileStorage file_storage;
    lldbg::OpenFiles open_files;
    std::unique_ptr<lldbg::FileTreeNode> file_browser;
    RenderState render_state;
    TextEditor text_editor;

    Application() = default;
    ~Application() { event_listener.stop(debugger); }

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application& operator=(Application&&) = delete;
};

// We only use a global variable because of how freeglut
// requires a frame update function with signature void(void).
// It is not used anywhere other than main_loop from main.cpp
extern Application g_application;

bool start_process(Application& app, const char* exe_filepath, const char** argv);
void kill_process(Application& app);
void pause_process(Application& app);
void continue_process(Application& app);
void tick(Application& app);
void handle_event(Application& app, lldb::SBEvent);
void draw(Application& app);

lldb::SBProcess get_process(Application& app);
}
