#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Application.hpp"
#include "Defer.hpp"
#include "FileSystem.hpp"
#include "Log.hpp"
#include "Timer.hpp"
#include "fmt/format.h"
#include "imgui.h"
#include "lldb/API/LLDB.h"

const std::string target_path = fmt::format("{}/test", LLDBG_TESTS_DIR);

using namespace lldbg;

int main(int argc, char** argv)
{
    (void)argv;

    // TODO: read in cmd line arguments, find location of '--' if exists, and split between
    // lldb/lldbg arguments and debugee program arguments
    for (int i = 0; i < argc; i++) {
        std::cout << argv[i] << std::endl;
    }

    Application app;
    app.argv = argv;

    TargetAddResult add_result = add_target(app, target_path);
    if (add_result != TargetAddResult::Success) {
        return EXIT_FAILURE;
    }

    TargetStartResult start_result = start_target(app);
    if (start_result != TargetStartResult::Success) {
        return EXIT_FAILURE;
    }

    set_workdir(app, LLDBG_TESTS_DIR);

    app.main_loop();

    return EXIT_SUCCESS;
}
