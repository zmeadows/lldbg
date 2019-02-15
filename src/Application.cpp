#include "Application.hpp"

#include "Log.hpp"
#include "Defer.hpp"

#include <chrono>
#include <iostream>
#include <queue>
#include <thread>
#include <assert.h>

#include "imgui.h"
#include <imgui_internal.h>
#include "examples/imgui_impl_freeglut.h"
#include "examples/imgui_impl_opengl2.h"
#include <GL/freeglut.h>

namespace lldbg {

bool start_process(Application& app, const char* exe_filepath, const char** argv) {
    app.debugger = lldb::SBDebugger::Create();
    app.debugger.SetAsync(true);
    app.command_line.replace_interpreter(app.debugger.GetCommandInterpreter());

    app.file_browser = FileTreeNode::create("/home/zac/lldbg");

    app.text_editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    app.text_editor.SetText(
        "int main(int argv, char** argv) {\n"
        "    std::cout << \"Hello, World!\" << std::endl;\n"
        "}\n"
        );
    app.text_editor.SetReadOnly(true);

    //TODO: convert all print statement below to Error-level logging

    //TODO: loop through running processes (if any) and kill them
    //and log information about it.
    lldb::SBError error;
    lldb::SBTarget target = app.debugger.CreateTarget(exe_filepath, nullptr, nullptr, true, error);
    if (!error.Success()) {
        std::cerr << "Error during target creation: " << error.GetCString() << std::endl;
        return false;
    }

    LOG(Debug) << "Succesfully created target for executable: " << exe_filepath;

    const lldb::SBFileSpec file_spec = target.GetExecutable();
    // const char* exe_name_spec = file_spec.GetFilename();
    // const char* exe_dir_spec = file_spec.GetDirectory();

    lldb::SBLaunchInfo launch_info(argv);
    launch_info.SetLaunchFlags(lldb::eLaunchFlagDisableASLR | lldb::eLaunchFlagStopAtEntry);
    lldb::SBProcess process = target.Launch(launch_info, error);

    if (!error.Success()) {
        std::cerr << "Failed to launch: " << exe_filepath << std::endl;
        std::cerr << "Error during launch: " << error.GetCString() << std::endl;
        return false;
    }

    LOG(Debug) << "Succesfully launched process for executable: " << exe_filepath;

    size_t ms_attaching = 0;
    while (process.GetState() == lldb::eStateAttaching) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ms_attaching += 100;
        if (ms_attaching/1000 > 5) {
            std::cerr << "Took >5 seconds to launch process, giving up!" << std::endl;
            return false;
        }
    }

    LOG(Debug) << "Succesfully attached to process for executable: " << exe_filepath;

    assert(app.command_line.run_command("settings set auto-confirm 1", true));
    assert(app.command_line.run_command("settings set target.x86-disassembly-flavor intel", true));
    assert(app.command_line.run_command("breakpoint set --file simple.cpp --line 5"));

    app.event_listener.start(app.debugger);

    get_process(app).Continue();

    return true;
}

void continue_process(Application& app) {
    lldb::SBProcess process = app.debugger.GetSelectedTarget().GetProcess();
    assert(process.IsValid());
    process.Continue();
}

void pause_process(Application& app) {
    lldb::SBProcess process = app.debugger.GetSelectedTarget().GetProcess();
    assert(process.IsValid());
    process.Stop();
}

void kill_process(Application& app) {
    lldb::SBProcess process = app.debugger.GetSelectedTarget().GetProcess();
    assert(process.IsValid());
    process.Kill();
}

lldb::SBProcess get_process(Application& app) {
    assert(app.debugger.GetNumTargets() <= 1);
    return app.debugger.GetSelectedTarget().GetProcess();
}

void manually_open_and_or_focus_file(Application& app, const std::string filepath) {
    if (app.open_files.open(filepath)) {
        app.render_state.request_manual_tab_change = true;
    }
}

}

