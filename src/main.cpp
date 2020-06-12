#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// TODO: remove un-used includes
// TODO: created Forward.hpp as in SerenityOS
#include "Application.hpp"
#include "Defer.hpp"
#include "FileSystem.hpp"
#include "Log.hpp"
#include "StringBuffer.hpp"
#include "Timer.hpp"
#include "cxxopts.hpp"
#include "fmt/format.h"
#include "imgui.h"
#include "lldb/API/LLDB.h"

int main(int argc, char** argv)
{
    // TODO: add version number to description here
    cxxopts::Options options("lldbg", "A GUI for the LLVM Debugger.");
    options.positional_help("");

    // TODO: make mutually exclusive arguments, for example attach vs. launch file
    // clang-format off
    options.allow_unrecognised_options().add_options()
        ("n,attach-name", "Tells the debugger to attach to a process with the given name.", cxxopts::value<std::string>())
        ("p,attach-pid", "Tells the debugger to attach to a process with the given pid.", cxxopts::value<std::string>())
        ("w,wait-for", "Tells the debugger to wait for a process with the given pid or name to launch before attaching.", cxxopts::value<std::string>())
        ("f,file", "Tells the debugger to use <filename> as the program to be debugged.", cxxopts::value<std::string>())
        ("S,source-before-file", "Tells the debugger to read in and execute the lldb  commands  in the given file, before any file has been loaded.", cxxopts::value<std::string>())
        ("workdir", "Specify base directory of file explorer tree", cxxopts::value<std::string>())
        ("h,help", "Print out usage information.")
        ("positional", "Positional arguments: these are the arguments that are entered without an option", cxxopts::value<std::vector<std::string>>())
        ;
    // clang-format on

    options.parse_positional({"file", "positional"});

    auto result = options.parse(argc, argv);

    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        return EXIT_SUCCESS;
    }

    if (auto lldb_error = lldb::SBDebugger::InitializeWithErrorHandling(); !lldb_error.Success()) {
        const char* lldb_error_cstr = lldb_error.GetCString();
        std::cerr << (lldb_error_cstr ? lldb_error_cstr : "Unknown LLDB error!");
        std::cerr << "Failed to initialize LLDB, exiting...";
        return EXIT_FAILURE;
    }
    Defer(lldb::SBDebugger::Terminate());

    Application app;

    if (result.count("workdir")) {
        const std::string workdir = result["workdir"].as<std::string>();
        app.set_workdir(workdir.c_str());
    }

    // TODO: test this
    if (result.count("source-before-file")) {
        const std::string source_path = result["source-before-file"].as<std::string>();
        auto handle = FileHandle::create(source_path);
        if (handle.has_value()) {
            for (const std::string& line : handle->contents()) {
                auto ret = app.session.run_lldb_command(line.c_str());
                if (!ret.IsValid()) {
                    LOG(Error) << "Error parsing source file line: " << line;
                    break;
                }
            }
            LOG(Debug) << "Successfully executed commands in source file: " << source_path;
        }
        else {
            LOG(Error) << "Invalid filepath passed to source-before-file argument: " << source_path;
        }
    }

    if (result.count("file")) {
        // TODO: detect and open main file of specified executable
        StringBuffer target_set_cmd;
        target_set_cmd.format("file {}", result["file"].as<std::string>());
        app.session.run_lldb_command(target_set_cmd.data());

        if (result.count("positional")) {
            StringBuffer argset_command;
            argset_command.format_("settings set target.run-args ");
            for (const auto& arg : result["positional"].as<std::vector<std::string>>()) {
                argset_command.format_("{} ", arg);
            }
            argset_command.format_("{}", '\0');
            app.session.run_lldb_command(argset_command.data());
        }
    }

    return app.main_loop();
}
