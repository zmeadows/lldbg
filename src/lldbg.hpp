#pragma once

#include "lldb/API/LLDB.h"

namespace lldbg {

using err_t = int;

struct lldbg {
    lldb::SBDebugger debugger;
    lldb::SBCommandInterpreter interpreter;
    lldb::SBTarget target;
    lldb::SBProcess process;
    lldb::SBListener listener;
    lldb::pid_t pid;
};

extern lldbg instance;
void initialize_instance(void);
void destroy_instance(void);

err_t continue_execution(lldbg& gui);

err_t run_command(lldbg& gui, const char* command);

void dump_state(lldbg& gui);

err_t process_events(lldbg& gui);

err_t launch(lldbg& gui, const char* full_exe_path, const char** argv);

err_t test(lldbg& gui, const char** const_argv_ptr);

}
