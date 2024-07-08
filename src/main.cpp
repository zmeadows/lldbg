#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// TODO: remove un-used includes
// TODO: create Forward.hpp as in SerenityOS
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
    options.add_options()
        ("n,attach-name", "Tells the debugger to attach to a process with the given name.", cxxopts::value<std::string>())
        ("p,attach-pid", "Tells the debugger to attach to a process with the given pid.", cxxopts::value<std::string>())
        ("w,wait-for", "Tells the debugger to wait for a process with the given pid or name to launch before attaching.", cxxopts::value<std::string>())
        ("f,file", "Tells the debugger to use <filename> as the program to be debugged.", cxxopts::value<std::string>())
        ("S,source-before-file", "Tells the debugger to read in and execute the lldb  commands  in the given file, before any file has been loaded.", cxxopts::value<std::string>())
        ("s,source", "Tells the debugger to read in and execute the lldb commands in the given file, after any file has been loaded.", cxxopts::value<std::string>())
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

    auto ui = UserInterface::init();
    if (!ui.has_value()) {
        LOG(Error) << "Failed to initialize graphics/UI.\n Exiting...";
        return EXIT_FAILURE;
    }

    std::optional<fs::path> workdir = {};
    if (result.count("workdir")) {
        fs::path workdir_request = fs::path(result["workdir"].as<std::string>());
        if (fs::exists(workdir_request) && fs::is_directory(workdir_request)) {
            workdir = workdir_request;
        }
    }

    Application app(*ui, workdir);

    if (result.count("source-before-file")) {
        const std::string source_path = result["source-before-file"].as<std::string>();
        auto handle = FileHandle::create(source_path);
        if (handle.has_value()) {
            for (const std::string& line : handle->contents()) {
                if (line.empty()) {
                    continue;
                }
                auto ret = run_lldb_command(app, line.c_str());
            }
            LOG(Verbose) << "Successfully executed commands in source file: " << source_path;
        }
        else {
            LOG(Error) << "Invalid filepath passed to source-before-file argument: " << source_path;
        }
    }

    if (result.count("file")) {
        // TODO: detect and open main file of specified executable
        StringBuffer target_set_cmd;
        target_set_cmd.format("file {}", result["file"].as<std::string>());
        run_lldb_command(app, target_set_cmd.data());

        if (result.count("positional")) {
            StringBuffer argset_command;
            argset_command.format_("settings set target.run-args ");
            for (const auto& arg : result["positional"].as<std::vector<std::string>>()) {
                argset_command.format_("{} ", arg);
            }
            argset_command.format_("{}", '\0');
            run_lldb_command(app, argset_command.data());
        }
    }

    if (result.count("source")) {
        const std::string source_path = result["source"].as<std::string>();
        auto handle = FileHandle::create(source_path);
        if (handle.has_value()) {
            for (const std::string& line : handle->contents()) {
                if (line.empty()) {
                    continue;
                }
                auto ret = run_lldb_command(app, line.c_str());
            }
            LOG(Verbose) << "Successfully executed commands in source file: " << source_path;
        }
        else {
            LOG(Error) << "Invalid filepath passed to --source argument: " << source_path;
        }
    }

    return main_loop(app);
}
