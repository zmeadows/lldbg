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

    const char* output = ret.GetOutput();
    if (output) {
        entry.output = std::string(ret.GetOutput());
    }

    entry.succeeded = ret.Succeeded();

    if (!hide_from_history) {
        m_history.emplace_back(entry);
    }

    return ret.Succeeded();
}

}
