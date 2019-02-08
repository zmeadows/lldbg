#include "lldbg.hpp"

#include "Log.hpp"

#include <chrono>
#include <iostream>
#include <queue>
#include <thread>
#include <assert.h>

#include <GL/freeglut.h>

namespace lldbg {

Application g_application;

bool Application::start_process(const char* exe_filepath, const char** argv) {
    m_debugger = lldb::SBDebugger::Create();
    m_debugger.SetAsync(true);
    m_command_line.replace_interpreter(m_debugger.GetCommandInterpreter());

    //TODO: loop through running processes (if any) and kill them
    //and log information about it.
    lldb::SBError error;
    lldb::SBTarget target = m_debugger.CreateTarget(exe_filepath, nullptr, nullptr, true, error);
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

    assert(m_command_line.run_command("settings set auto-confirm 1"));
    assert(m_command_line.run_command("settings set target.x86-disassembly-flavor intel"));
    assert(m_command_line.run_command("breakpoint set --file simple.cpp --line 5"));

    m_event_listener.start(m_debugger);

    get_process().Continue();

    return true;
}

void Application::continue_process() {
    lldb::SBProcess process = m_debugger.GetSelectedTarget().GetProcess();
    assert(process.IsValid());
    process.Continue();
}

void Application::pause_process() {
    lldb::SBProcess process = m_debugger.GetSelectedTarget().GetProcess();
    assert(process.IsValid());
    process.Stop();
}

void Application::kill_process() {
    lldb::SBProcess process = m_debugger.GetSelectedTarget().GetProcess();
    assert(process.IsValid());
    process.Kill();
}

void dump_state(lldb::SBProcess process) {
    for (auto thread_idx = 0; thread_idx < process.GetNumThreads(); thread_idx++) {
        lldb::SBThread th = process.GetThreadAtIndex(thread_idx);
        std::cout << "Thread " << thread_idx << ": " << th.GetName() << std::endl;

        for (auto frame_idx = 0; frame_idx < th.GetNumFrames(); frame_idx++) {
            lldb::SBFrame fr = th.GetFrameAtIndex(frame_idx);
            const char* function_name = fr.GetDisplayFunctionName();
            std::cout << "\tFrame " << frame_idx << ": " << function_name;

            if (function_name[0] != '_') {
                lldb::SBLineEntry line_entry = fr.GetLineEntry();
                const char* file_path = line_entry.GetFileSpec().GetFilename();
                std::cout << " --> " << file_path << " @ line " << line_entry.GetLine() << ", column " << line_entry.GetColumn();

                std::cout << fr.Disassemble() << std::endl;
            }

            std::cout << std::endl;
        }
    }
}

// err_t test(Application& gui, const char** const_argv_ptr) {
//     err_t ret;
//
//     ret = run_command(gui, "settings set auto-confirm 1");
//     if (ret != 0) { return EXIT_FAILURE; }
//
//     ret = run_command(gui, "settings set target.x86-disassembly-flavor intel");
//     if (ret != 0) { return EXIT_FAILURE; }
//
//     ret = launch(gui, "/home/zac/lldbg/test/a.out" , const_argv_ptr);
//     if (ret != 0) { return EXIT_FAILURE; }
//
//     ret = run_command(gui, "breakpoint set --file simple.cpp --line 5");
//     if (ret != 0) { return EXIT_FAILURE; }
//
//     ret = continue_execution(gui);
//     if (ret != 0) { return EXIT_FAILURE; }
//
//     ret = process_events(gui);
//     if (ret != 0) { return EXIT_FAILURE; }
//
//     return 0;
// }

}
