#pragma once

#include "lldb/API/LLDB.h"

#include "renderer.hpp"
#include "Log.hpp"
#include "FileSystem.hpp"

#include "LLDBEventListenerThread.hpp"
#include "LLDBCommandLine.hpp"

#include <iostream>
#include <assert.h>

namespace lldbg {

using err_t = int;

void dump_state(lldb::SBProcess process);

//TODO: just make this a simple struct?
class Application {
    lldb::SBDebugger m_debugger;
    lldbg::LLDBEventListenerThread m_event_listener;
    lldbg::LLDBCommandLine m_command_line;
    lldbg::FileBrowser m_filesystem;

    RenderState m_render_state;

    lldb::SBProcess get_process() {
        assert(m_debugger.GetNumTargets() <= 1);
        return m_debugger.GetSelectedTarget().GetProcess();
    }

    void handle_event(lldb::SBEvent);
    void kill_process();
    void pause_process();
    void continue_process();

public:
    bool start_process(const char* exe_filepath, const char** argv);
    void tick(void);

    Application() = default;
    ~Application() { m_event_listener.stop(m_debugger); }

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application& operator=(Application&&) = delete;
};

// We only use a global variable because of how freeglut
// requires a frame update function with signature void(void).
// It is not used anywhere other than main_loop from main.cpp
extern Application g_application;

}
