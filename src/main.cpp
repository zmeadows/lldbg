#include <iostream>

#include "lldb/API/LLDB.h"

// dear imgui: standalone example application for FreeGLUT + OpenGL2, using legacy fixed pipeline
// If you are new to dear imgui, see examples/README.txt and documentation at the top of imgui.cpp.
// (Using GLUT or FreeGLUT is not recommended unless you really miss the 90's)

#include "imgui.h"
#include "examples/imgui_impl_freeglut.h"
#include "examples/imgui_impl_opengl2.h"
#include <GL/freeglut.h>

#include <queue>
#include <chrono>
#include <thread>

#ifdef _MSC_VER
#pragma warning (disable: 4505) // unreferenced local function has been removed
#endif

static bool show_demo_window = true;
static bool show_another_window = false;
static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

void my_display_code()
{
    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
        ImGui::Checkbox("Another Window", &show_another_window);

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();
    }
}

void glut_display_func()
{
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplFreeGLUT_NewFrame();

    my_display_code();

    // Rendering
    ImGui::Render();
    ImGuiIO& io = ImGui::GetIO();
    glViewport(0, 0, (GLsizei)io.DisplaySize.x, (GLsizei)io.DisplaySize.y);
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    //glUseProgram(0); // You may want this if using this code in an OpenGL 3+ context where shaders may be bound, but prefer using the GL3+ code.
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

    glutSwapBuffers();
    glutPostRedisplay();
}

// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.

namespace lldbgui {

class LLDBSentry {
public:
  LLDBSentry() {
    // Initialize LLDB
      lldb::SBDebugger::Initialize();
  }
  ~LLDBSentry() {
    // Terminate LLDB
      lldb::SBDebugger::Terminate();
  }
};

using err_t = int;

struct lldbgui {
    lldb::SBDebugger debugger;
    lldb::SBCommandInterpreter interpreter;
    lldb::SBTarget target;
    lldb::pid_t pid;
    lldb::SBProcess process;
    lldb::SBListener listener;

    lldbgui(void);

    lldbgui(const lldbgui&) = delete;
    lldbgui(lldbgui&&) = delete;
    lldbgui& operator=(const lldbgui&) = delete;
    lldbgui& operator=(lldbgui&&) = delete;
};

lldbgui::lldbgui(void)
    : debugger(lldb::SBDebugger::Create())
    , interpreter(debugger.GetCommandInterpreter())
{
    this->debugger.SetAsync(true);
}

err_t continue_execution(lldbgui& gui) {
    if (!gui.process.IsValid()) {
        std::cerr << "process invalid" << std::endl;
        return -1;
    }

    gui.process.Continue();
    return 0;
}

err_t run_command(lldbgui& gui, const char* command) {
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

void dump_state(lldbgui& gui) {
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

err_t process_events(lldbgui& gui) {
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

err_t launch(lldbgui& gui, const char* full_exe_path, const char** argv) {
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


}

int main(int argc, char** argv)
{
    lldbgui::LLDBSentry sentry;

    std::vector<std::string> args( argv + 1, argv + argc );
    std::vector<const char*> const_argv;

    for (const std::string& arg : args) {
        const_argv.push_back(arg.c_str());
    }

    const char** const_argv_ptr = const_argv.data();

    lldbgui::lldbgui gui;
    int ret;

    ret = run_command(gui, "settings set auto-confirm 1");
    if (ret != 0) { return EXIT_FAILURE; }

    ret = run_command(gui, "settings set target.x86-disassembly-flavor intel");
    if (ret != 0) { return EXIT_FAILURE; }

    ret = lldbgui::launch(gui, "/home/zac/lldbgui/test/a.out" , const_argv_ptr);
    if (ret != 0) { return EXIT_FAILURE; }

    ret = run_command(gui, "breakpoint set --file simple.cpp --line 5");
    if (ret != 0) { return EXIT_FAILURE; }

    ret = lldbgui::continue_execution(gui);
    if (ret != 0) { return EXIT_FAILURE; }

    ret = lldbgui::process_events(gui);
    if (ret != 0) { return EXIT_FAILURE; }

    return 0;

    // Create GLUT window
    glutInit(&argc, argv);
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_MULTISAMPLE);
    glutInitWindowSize(1280, 720);
    glutCreateWindow("Dear ImGui FreeGLUT+OpenGL2 Example");

    // Setup GLUT display function
    // We will also call ImGui_ImplFreeGLUT_InstallFuncs() to get all the other functions installed for us,
    // otherwise it is possible to install our own functions and call the imgui_impl_freeglut.h functions ourselves.
    glutDisplayFunc(glut_display_func);

    // Setup Dear ImGui context
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer bindings
    ImGui_ImplFreeGLUT_Init();
    ImGui_ImplFreeGLUT_InstallFuncs();
    ImGui_ImplOpenGL2_Init();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'misc/fonts/README.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    glutMainLoop();

    // Cleanup
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplFreeGLUT_Shutdown();
    ImGui::DestroyContext();

    return 0;
}
