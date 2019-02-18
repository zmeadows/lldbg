#include "LLDBCommandLine.hpp"

#include "Log.hpp"

namespace lldbg {

bool LLDBCommandLine::run_command(const char* command, bool hide_from_history)
{
    if (!command) {
        LOG(Error) << "Attempted to run empty command!";
        return false;
    }

    CommandLineEntry entry;
    entry.input = std::string(command);

    lldb::SBCommandReturnObject ret;
    m_interpreter.HandleCommand(command, ret);

    if (ret.GetOutput()) {
        entry.output = std::string(ret.GetOutput());
    }

    entry.succeeded = ret.Succeeded();

    if (!entry.succeeded) {
        if (ret.GetError()) {
            entry.error_msg = std::string(ret.GetError());
        } else {
            entry.error_msg = "Unknown failure reason!";
        }
    }

    if (!hide_from_history) {
        m_history.emplace_back(entry);
    }

    return ret.Succeeded();
}

}
