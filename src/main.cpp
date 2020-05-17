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

int main(int, char* argv[])
{
    Application app;

    TargetAddResult add_result = add_target(app, target_path);
    if (add_result != TargetAddResult::Success) {
        return EXIT_FAILURE;
    }

    TargetStartResult start_result = start_target(app, const_cast<const char**>(argv));
    if (start_result != TargetStartResult::Success) {
        return EXIT_FAILURE;
    }

    set_workdir(app, LLDBG_TESTS_DIR);

    app.main_loop();

    return EXIT_SUCCESS;
}
