#pragma once

#include <optional>
#include <string>
#include <vector>

#include "lldb/API/LLDB.h"

// TODO: convert to variant to represent either user command, stdout, sterr, or log
// message
struct CommandLineEntry {
    std::string input;
    std::string output;
    std::string error_msg;  // TODO: use std::optional and eliminate 'succeeded' bool
    bool succeeded;
};

class LLDBCommandLine {
    lldb::SBCommandInterpreter m_interpreter;
    std::vector<CommandLineEntry> m_history;

public:
    LLDBCommandLine(lldb::SBDebugger& debugger);

    lldb::SBCommandReturnObject run_command(const char* command,
                                            bool hide_from_history = false);
    std::optional<std::string> expand_and_unalias_command(const char* command);

    // TODO: for_each_history_entry
    const std::vector<CommandLineEntry>& get_history() const { return m_history; }
};
