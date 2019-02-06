#include "lldbg.hpp"

#include <chrono>
#include <iostream>
#include <queue>
#include <thread>

namespace lldbg {

lldbg instance;

void initialize_instance(void) {
    instance.debugger = lldb::SBDebugger::Create();
    instance.interpreter = instance.debugger.GetCommandInterpreter();

    instance.debugger.SetAsync(true);
}

void destroy_instance(void) {
    lldb::SBDebugger::Destroy(instance.debugger);
}

err_t continue_execution(lldbg& gui) {
    if (!gui.process.IsValid()) {
        std::cerr << "process invalid" << std::endl;
        return -1;
    }

    gui.process.Continue();
    return 0;
}

err_t run_command(lldbg& gui, const char* command) {
    lldb::SBCommandReturnObject return_object;
    gui.interpreter.HandleCommand(command, return_object);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (return_object.Succeeded()) {
        std::cout << "command succeeded: " << command << std::endl;
        if (return_object.GetOutput()) {
            std::cout << "output:" << std::endl;
            std::cout << return_object.GetOutput() << std::endl;
        }
        return 0;
    } else {
        std::cerr << "Failed to run command: " << command << std::endl;
        return -1;
    }
}

void dump_state(lldbg& gui) {
    for (auto thread_idx = 0; thread_idx < gui.process.GetNumThreads(); thread_idx++) {
        lldb::SBThread th = gui.process.GetThreadAtIndex(thread_idx);
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

err_t process_events(lldbg& gui) {
    lldb::SBEvent event;

    const lldb::StateType old_state = gui.process.GetState();

    if (old_state == lldb::eStateInvalid || old_state == lldb::eStateExited) {
        return -1;
    }

    int count = 0;
    while (true) {
        if (count > 100) {
            std::cout << "hit polling limit." << std::endl;
            break;
        }

        if (gui.listener.WaitForEvent(1, event)) {
            const lldb::StateType new_state = lldb::SBProcess::GetStateFromEvent(event);
            std::cout << "event polled with state: " << lldb::SBDebugger::StateAsCString(new_state) << std::endl;

            if (!event.IsValid()) {
                std::cout << "Invalid event!" << std::endl;
                return -1;
            }

            if (new_state == lldb::eStateStopped) {
                std::cout << "Stopped (maybe at breakpoint?)." << std::endl;
                dump_state(gui);
                std::cout << "Continuing..." << std::endl;
                gui.process.Continue();
            } else if (new_state == lldb::eStateExited) {
                auto exit_desc = gui.process.GetExitDescription();
                std::cout << "Exiting with status: " << gui.process.GetExitStatus();
                if (exit_desc) {
                    std::cout << " and description " << exit_desc;
                }
                std::cout << std::endl;
                return 0;
            }
        }

        count++;
    }

    return 0;
}

err_t launch(lldbg& gui, const char* full_exe_path, const char** argv) {
    //TODO: add program args to launch
    lldb::SBError error;

    gui.target = gui.debugger.CreateTarget(full_exe_path, nullptr, nullptr, true, error);
    if (!error.Success()) {
        std::cerr << "Error during target creation: " << error.GetCString() << std::endl;
        return -1;
    }

    const lldb::SBFileSpec file_spec = gui.target.GetExecutable();
    const char* exe_name_spec = file_spec.GetFilename();
    const char* exe_dir_spec = file_spec.GetDirectory();

    lldb::SBLaunchInfo launch_info(argv);
    launch_info.SetLaunchFlags(lldb::eLaunchFlagDisableASLR | lldb::eLaunchFlagStopAtEntry);
    gui.process = gui.target.Launch(launch_info, error);

    while (gui.process.GetState() == lldb::eStateAttaching) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!error.Success()) {
        std::cerr << "Failed to launch: " << full_exe_path << std::endl;
        std::cerr << "Error during launch: " << error.GetCString() << std::endl;
        return -1;
    }


    gui.pid = gui.process.GetProcessID();

    if (gui.pid == LLDB_INVALID_PROCESS_ID) {
        std::cerr << "Invalid PID: " << gui.pid << std::endl;
        return -1;
    }

    gui.listener = gui.debugger.GetListener();
    gui.process.GetBroadcaster().AddListener(gui.listener,
                                               lldb::SBProcess::eBroadcastBitStateChanged
                                             | lldb::SBProcess::eBroadcastBitSTDOUT
                                             | lldb::SBProcess::eBroadcastBitSTDERR
                                             );

    std::cout << "Launched " << exe_dir_spec << "/" << exe_name_spec << std::endl;

    return 0;
}

err_t test(lldbg& gui, const char** const_argv_ptr) {
    err_t ret;

    ret = run_command(gui, "settings set auto-confirm 1");
    if (ret != 0) { return EXIT_FAILURE; }

    ret = run_command(gui, "settings set target.x86-disassembly-flavor intel");
    if (ret != 0) { return EXIT_FAILURE; }

    ret = launch(gui, "/home/zac/lldbg/test/a.out" , const_argv_ptr);
    if (ret != 0) { return EXIT_FAILURE; }

    ret = run_command(gui, "breakpoint set --file simple.cpp --line 5");
    if (ret != 0) { return EXIT_FAILURE; }

    ret = continue_execution(gui);
    if (ret != 0) { return EXIT_FAILURE; }

    ret = process_events(gui);
    if (ret != 0) { return EXIT_FAILURE; }

    return 0;
}

}
