#pragma once

#include "lldb/API/LLDB.h"

#include <iostream>

namespace lldbg {

using err_t = int;

class LLDBCommandLine {
    lldb::SBCommandInterpreter m_interpreter;
    std::vector<std::string> m_input_history;
    std::vector<std::string> m_output_history;
public:

    LLDBCommandLine(lldb::SBCommandInterpreter&);

    bool run_command(const char* command) {
        //TODO: log nullptr
        if (!command) return;
        m_input_history.emplace_back(command);

        lldb::SBCommandReturnObject return_object;
        m_interpreter.HandleCommand(command, return_object);
        if (return_object.Succeeded()) {
            const char* output = return_object.GetOutput();
            if (output) {
                m_output_history.emplace_back(return_object.GetOutput());
            }
        }
    }
};

class Application {
    lldb::SBDebugger m_debugger;
    LLDBEventListenerThread m_event_listener;
    LLDBCommandLine m_command_line;

    std::vector<std::string> m_stdout;

    Renderer m_renderer;

    void stop_process();
    void pause_process();
    void continue_process();

public:
    bool start_process(const char* exe_filepath, const char** argv);

    void tick(void) {
        m_renderer.draw(m_debugger.GetSelectedTarget().GetProcess());
    }

    Application(int* argcp, char** argv)
        : m_renderer()
        , m_debugger(lldb::SBDebugger::Create())
        , m_interpreter(m_debugger.GetCommandInterpreter())
        , m_event_listener()
    {
        m_debugger.SetAsync(true);
    }

    Application() = delete;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application& operator=(Application&&) = delete;
};

extern Application g_application;


}
