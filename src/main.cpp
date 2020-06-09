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
#include "Timer.hpp"
#include "cxxopts.hpp"
#include "fmt/format.h"
#include "imgui.h"
#include "lldb/API/LLDB.h"

using namespace lldbg;

int main(int argc, char** argv)
{
    // TODO: add version number to description here
    cxxopts::Options options("lldbg", "A GUI for the LLVM Debugger.");
    options.positional_help("");

    // clang-format off
    options.allow_unrecognised_options().add_options()
        ("n,attach-name", "Tells the debugger to attach to a process with the given name.", cxxopts::value<std::string>())
        ("p,attach-pid", "Tells the debugger to attach to a process with the given pid.", cxxopts::value<std::string>())
        ("w,wait-for", "Tells the debugger to wait for a process with the given pid or name to launch before attaching.", cxxopts::value<std::string>())
        ("f,file", "Tells the debugger to use <filename> as the program to be debugged.", cxxopts::value<std::string>())
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

    std::optional<std::string> exe_filepath;
    if (result.count("file")) {
        exe_filepath = result["file"].as<std::string>();
    }

    std::vector<std::string> exe_args;
    if (result.count("positional")) {
        exe_args = result["positional"].as<std::vector<std::string>>();
    }

    Application app;

    if (exe_filepath.has_value()) {
        const std::string target_path = fmt::format("{}/test", LLDBG_TESTS_DIR);
        app.session = DebugSession::create(target_path, exe_args, lldb::eLaunchFlagStopAtEntry);

        if (app.session == nullptr) {
            std::cerr << "failed to launch test target";
            return EXIT_FAILURE;
        }
        // TODO: set workdir from cmd line args, or cwd if unspecified
        set_workdir(app, LLDBG_TESTS_DIR);

        // assert(app.session->start_process());
    }

    return app.main_loop();
}
