#include <iostream>

#include "lldb/API/LLDB.h"

#include "Application.hpp"
#include "Defer.hpp"
#include "FileSystem.hpp"
#include "Log.hpp"
#include "Timer.hpp"

#include "imgui.h"

#include <fstream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    std::cout << __LINE__ << std::endl;
    lldbg::g_application = std::make_unique<lldbg::Application>();
    std::cout << __LINE__ << std::endl;
    lldbg::g_logger = std::make_unique<lldbg::Logger>();

    std::cout << (void*)lldbg::g_application.get() << std::endl;

    std::vector<std::string> args(argv + 1, argv + argc);
    std::vector<const char*> const_argv;

    for (const std::string& arg : args) {
        const_argv.push_back(arg.c_str());
    }

    const char** const_argv_ptr = const_argv.data();

    auto err =
        lldbg::create_new_target(*lldbg::g_application, "/home/zmeadows/Downloads/lldbg/test/test",
                                 const_argv_ptr, true, "/home/zmeadows/Downloads/lldbg/test/");

    if (err) {
        std::cout << err->msg << std::endl;
    }
    else {
        lldbg::g_application->main_loop();
    }

    std::cout << __LINE__ << std::endl;

    // NOTE: important to destruct these in order, for now
    lldbg::g_application.reset(nullptr);
    lldbg::g_logger.reset(nullptr);

    return 0;
}
