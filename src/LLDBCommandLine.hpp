#pragma once

#include <string>
#include <vector>

#include "lldb/API/LLDB.h"

namespace lldbg {

// TODO: rename LLDBCommandLineEntry
struct CommandLineEntry {
    std::string input;
    std::string output;
    std::string error_msg;  // TODO: use std::optional
    bool succeeded;
};

class LLDBCommandLine {
    // TODO: disable colorized output
    lldb::SBCommandInterpreter m_interpreter;
    std::vector<CommandLineEntry> m_history;

public:
    LLDBCommandLine(lldb::SBDebugger& debugger);

    lldb::SBReturnStatus run_command(const char* command, bool hide_from_history = false);

    const std::vector<CommandLineEntry>& get_history() const { return m_history; }
};

}  // namespace lldbg
