#pragma once

#include "lldb/API/LLDB.h"

#include "renderer.hpp"

#include "LLDBEventListenerThread.hpp"

#include <iostream>

namespace lldbg {

using err_t = int;

class LLDBCommandLine {
    lldb::SBCommandInterpreter m_interpreter;
    std::vector<std::string> m_input_history;
    std::vector<std::string> m_output_history;
public:

    LLDBCommandLine(lldb::SBCommandInterpreter interpreter) : m_interpreter(interpreter) {}

    bool run_command(const char* command) {
        //TODO: log nullptr
        if (!command) return false;
        m_input_history.emplace_back(command);

        lldb::SBCommandReturnObject ret;
        m_interpreter.HandleCommand(command, ret);

        if (ret.Succeeded()) {
            const char* output = ret.GetOutput();
            if (output) {
                m_output_history.emplace_back(ret.GetOutput());
            }
            return true;
        }

        return false;
    }
};

class Application {
    lldb::SBDebugger m_debugger;
    lldbg::LLDBEventListenerThread m_event_listener;
    lldbg::LLDBCommandLine m_command_line;

    lldb::SBProcess process() { return m_debugger.GetSelectedTarget().GetProcess(); }

    void handle_event(lldb::SBEvent);

public:
    bool start_process(const char* exe_filepath, const char** argv);
    void kill_process();
    void pause_process();
    void continue_process();

    void tick(void) {
        while (m_event_listener.events_are_waiting()) {
            lldb::SBEvent event = m_event_listener.pop_event();
        }
        draw(process());
    }

    Application()
        : m_debugger(lldb::SBDebugger::Create())
        , m_event_listener()
        , m_command_line(m_debugger.GetCommandInterpreter())
    {
        m_debugger.SetAsync(true);
    }

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application& operator=(Application&&) = delete;
};

// We only use a global variable because of how freeglut
// requires a frame update function with signature void(void).
// It is not used anywhere other than main_loop from main.cpp
extern Application g_application;

void dump_state(lldb::SBProcess process);

}
