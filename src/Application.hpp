#pragma once

#include "lldb/API/LLDB.h"

#include "Log.hpp"
#include "FileSystem.hpp"

#include "LLDBEventListenerThread.hpp"
#include "LLDBCommandLine.hpp"

#include <iostream>
#include <assert.h>

namespace lldbg {

using err_t = int;

struct RenderState {
    int viewed_thread_index = -1;
    int viewed_frame_index = -1;
    int window_width = -1;
    int window_height = -1;
    std::string viewed_file;
};

struct Application {
    lldb::SBDebugger debugger;
    lldbg::LLDBEventListenerThread event_listener;
    lldbg::LLDBCommandLine command_line;
    lldbg::FileStorage file_storage;
    std::unique_ptr<lldbg::FileTreeNode> file_browser;
    const std::vector<std::string>* viewed_file;
    RenderState render_state;

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
