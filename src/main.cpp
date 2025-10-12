#include "Application.hpp"
#include "Defer.hpp"
#include "FileSystem.hpp"
#include "Log.hpp"
#include "StringBuffer.hpp"
#include "cxxopts.hpp"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    try
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
            ("loglevel", "Set the log level (debug, verbose, info, warning, error)", cxxopts::value<std::string>())
            ("h,help", "Print out usage information.")
            ("positional", "Positional arguments: these are the arguments that are entered without an option", cxxopts::value<std::vector<std::string>>())
            ;
        // clang-format on

        options.parse_positional({"file", "positional"});

        auto result = options.parse(argc, argv);

        if (result.count("help") > 0)
        {
            std::cout << options.help() << '\n';
            return EXIT_SUCCESS;
        }

        if (result.count("loglevel") > 0)
        {
            const std::string loglevel = result["loglevel"].as<std::string>();
            if (loglevel == "debug")
            {
                Logger::get_instance()->set_log_level((int) LogLevel::Debug);
            }
            else if (loglevel == "verbose")
            {
                Logger::get_instance()->set_log_level((int) LogLevel::Verbose);
            }
            else if (loglevel == "info")
            {
                Logger::get_instance()->set_log_level((int) LogLevel::Info);
            }
            else if (loglevel == "warning")
            {
                Logger::get_instance()->set_log_level((int) LogLevel::Warning);
            }
            else if (loglevel == "error")
            {
                Logger::get_instance()->set_log_level((int) LogLevel::Error);
            }
            else
            {
                LOG(Error) << "Invalid log level specified: " << loglevel;
                return EXIT_FAILURE;
            }
            LOG(Verbose) << "Setting log level to: " << loglevel;
        }

        if (auto lldb_error = lldb::SBDebugger::InitializeWithErrorHandling();
            !lldb_error.Success())
        {
            const char* lldb_error_cstr = lldb_error.GetCString();
            std::cerr << (lldb_error_cstr != nullptr ? lldb_error_cstr : "Unknown LLDB error!");
            std::cerr << "Failed to initialize LLDB, exiting...";
            return EXIT_FAILURE;
        }
        Defer(lldb::SBDebugger::Terminate());

        auto ui = UserInterface::init();
        if (!ui.has_value())
        {
            LOG(Error) << "Failed to initialize graphics/UI.\n Exiting...";
            return EXIT_FAILURE;
        }

        std::optional<fs::path> workdir = {};
        if (result.count("workdir") > 0)
        {
            fs::path workdir_request = fs::path(result["workdir"].as<std::string>());
            if (fs::exists(workdir_request) && fs::is_directory(workdir_request))
            {
                workdir = workdir_request;
            }
        }

        Application app(*ui, workdir);

        if (result.count("source-before-file") > 0)
        {
            const std::string source_path = result["source-before-file"].as<std::string>();
            auto handle = FileHandle::create(source_path);
            if (handle.has_value())
            {
                for (const std::string& line : handle->contents())
                {
                    if (line.empty())
                    {
                        continue;
                    }
                    auto ret = run_lldb_command(app, line.c_str());
                }
                LOG(Verbose) << "Successfully executed commands in source file: " << source_path;
            }
            else
            {
                LOG(Error) << "Invalid filepath passed to source-before-file argument: "
                           << source_path;
            }
        }

        if (result.count("file") > 0)
        {
            // TODO: detect and open main file of specified executable
            StringBuffer target_set_cmd;
            target_set_cmd.format("file {}", result["file"].as<std::string>());
            run_lldb_command(app, target_set_cmd.data());

            if (result.count("positional") > 0)
            {
                StringBuffer argset_command;
                argset_command.format_("settings set target.run-args ");
                for (const auto& arg : result["positional"].as<std::vector<std::string>>())
                {
                    argset_command.format_("{} ", arg);
                }
                argset_command.format_("{}", '\0');
                run_lldb_command(app, argset_command.data());
            }
        }

        if (result.count("source") > 0)
        {
            const std::string source_path = result["source"].as<std::string>();
            auto handle = FileHandle::create(source_path);
            if (handle.has_value())
            {
                for (const std::string& line : handle->contents())
                {
                    if (line.empty())
                    {
                        continue;
                    }
                    auto ret = run_lldb_command(app, line.c_str());
                }
                LOG(Verbose) << "Successfully executed commands in source file: " << source_path;
            }
            else
            {
                LOG(Error) << "Invalid filepath passed to --source argument: " << source_path;
            }
        }

        return main_loop(app);
    }
    catch (const std::exception& e)
    {
        // Catch other standard exceptions
        std::cerr << "An unexpected error occurred: " << e.what() << '\n';
        return 1;
    }
}
