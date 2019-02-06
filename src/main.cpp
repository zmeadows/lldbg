#include <iostream>

#include "lldb/API/LLDB.h"

#include "lldbg.hpp"
#include "renderer.hpp"
#include "Defer.hpp"

#include <GL/freeglut.h>

int main(int argc, char** argv)
{

    lldb::SBDebugger::Initialize();
    lldbg::initialize_instance();
    lldbg::initialize_renderer(argc, argv);

    Defer(
        lldb::SBDebugger::Terminate();
        lldbg::destroy_instance();
        lldbg::destroy_renderer();
        );

    std::vector<std::string> args( argv + 1, argv + argc );
    std::vector<const char*> const_argv;

    for (const std::string& arg : args) {
        const_argv.push_back(arg.c_str());
    }

    const char** const_argv_ptr = const_argv.data();

    test(lldbg::instance, const_argv_ptr);

    glutMainLoop();

    return 0;
}
