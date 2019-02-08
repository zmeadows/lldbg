#pragma once

#include "lldb/API/LLDB.h"

#include "renderer.hpp"
#include "Log.hpp"

#include "LLDBEventListenerThread.hpp"

#include <iostream>
#include <assert.h>

namespace lldbg {

using err_t = int;

void dump_state(lldb::SBProcess process);

class LLDBCommandLine {
    lldb::SBCommandInterpreter m_interpreter;
    std::vector<std::string> m_input_history;
    std::vector<std::string> m_output_history;
public:

    void replace_interpreter(lldb::SBCommandInterpreter interpreter) { m_interpreter = interpreter; }

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
                std::cout << m_output_history.back() << std::endl;
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

    lldb::SBProcess get_process() {
        assert(m_debugger.GetNumTargets() <= 1);
        return m_debugger.GetSelectedTarget().GetProcess();
    }

    void handle_event(lldb::SBEvent);

public:
    bool start_process(const char* exe_filepath, const char** argv);
    void kill_process();
    void pause_process();
    void continue_process();

    void tick(void) {
        lldb::SBEvent event;

        while (m_event_listener.events_are_waiting()) {
            event = m_event_listener.pop_event();
            LOG(Debug) << "Found event with new state: " << lldb::SBDebugger::StateAsCString(lldb::SBProcess::GetStateFromEvent(event));
        }
        // dump_state(get_process());
        //assert(get_process().GetState() == lldb::SBProcess::GetStateFromEvent(event));

        draw(get_process());
    }

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
