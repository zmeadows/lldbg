#pragma once

#include "lldb/API/LLDB.h"

#include <vector>
#include <string>

namespace lldbg {

struct CommandLineEntry final {
    std::string input;
    std::string output;
    std::optional<std::string> error_msg;
    bool succeeded;
};

class LLDBCommandLine final {
    lldb::SBCommandInterpreter m_interpreter;
    std::vector<CommandLineEntry> m_history;
public:

    void replace_interpreter(lldb::SBCommandInterpreter interpreter) {
        m_interpreter = interpreter;
        m_history.clear();
    }

    bool run_command(const char* command, bool hide_from_history = false);

    const std::vector<CommandLineEntry>& get_history() const { return m_history; }
};

}
