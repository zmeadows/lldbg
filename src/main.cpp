#include <iostream>

#include "lldb/API/LLDB.h"

#include "Application.hpp"
#include "Defer.hpp"
#include "FileSystem.hpp"
#include "Log.hpp"
#include "Timer.hpp"

#include <GL/freeglut.h>
#include "examples/imgui_impl_freeglut.h"
#include "examples/imgui_impl_opengl2.h"
#include "imgui.h"

#include <fstream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    lldbg::g_application = std::make_unique<lldbg::Application>(&argc, argv);
    lldbg::g_logger = std::make_unique<lldbg::Logger>();

    std::vector<std::string> args(argv + 1, argv + argc);
    std::vector<const char*> const_argv;

    for (const std::string& arg : args) {
        const_argv.push_back(arg.c_str());
    }

    const char** const_argv_ptr = const_argv.data();

    auto err = lldbg::create_new_target(*lldbg::g_application,
                                        "/home/zac/lldbg/test/a.out",
                                        const_argv_ptr, true,
                                        "/home/zac/lldbg/test/"
                                        );

    if (err) {
        std::cout << err->msg << std::endl;
        return 1;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();
    // TODO: read font path from CMake-defined variable
    lldbg::g_application->render_state.font =
        io.Fonts->AddFontFromFileTTF("../lib/imgui/misc/fonts/Hack-Regular.ttf", 15.0f);

    glutMainLoop();

    // NOTE: important to destruct these in order, for now
    lldbg::g_application.reset(nullptr);
    lldbg::g_logger.reset(nullptr);

    return 0;
}
