#include "LLDBCommandLine.hpp"

#include "Log.hpp"

// TODO: smart user completion using the HandleCompletionWithDescriptions functions

LLDBCommandLine::LLDBCommandLine(lldb::SBDebugger& debugger)
    : m_interpreter(debugger.GetCommandInterpreter())
{
    run_command("settings set auto-confirm 1", true);

    // TODO: let use choose disassembly flavor in drop down menu
    run_command("settings set target.x86-disassembly-flavor intel", true);
}

lldb::SBCommandReturnObject LLDBCommandLine::run_command(const char* command,
                                                         bool hide_from_history)
{
    if (!command) {
        LOG(Warning) << "Attempted to run empty command!";
        auto ret = lldb::SBCommandReturnObject();
        ret.SetStatus(lldb::eReturnStatusInvalid);
        return ret;
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
        }
        else {
            entry.error_msg = "Unknown failure reason!";
        }
    }

    if (!hide_from_history) {
        m_history.emplace_back(std::move(entry));
    }

    return ret;
}

std::optional<std::string> LLDBCommandLine::expand_and_unalias_command(const char* command)
{
    if (!command) {
        LOG(Warning) << "Attempted to expand/unalias empty command!";
        return {};
    }

    lldb::SBCommandReturnObject ret;
    m_interpreter.ResolveCommand(command, ret);

    const char* output = ret.GetOutput();
    if (ret.Succeeded() && output) {
        return std::string(output);
    }
    else {
        return {};
    }
}
