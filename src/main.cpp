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

int main(int, char* argv[])
{
    lldbg::Application app;

    const std::string target_path = fmt::format("{}/test", LLDBG_TESTS_DIR);
    auto err = lldbg::create_new_target(app, target_path.c_str(),
                                        const_cast<const char**>(argv), true, LLDBG_TESTS_DIR);

    if (err) {
        std::cerr << err->msg << std::endl;
    }
    else {
        app.main_loop();
    }

    return 0;
}
