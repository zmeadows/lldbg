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
    static float f = 0.0f;
    static int counter = 0;

    const int window_width = glutGet(GLUT_WINDOW_WIDTH);
    const int window_height = glutGet(GLUT_WINDOW_HEIGHT);

    ImGui::SetNextWindowPos(ImVec2(0.f,0.f), ImGuiSetCond_Always);
    ImGui::SetNextWindowSize(ImVec2(window_width, window_height), ImGuiSetCond_Always);
    // Create a window called "Hello, world!" and append into it.
    ImGui::Begin("Hello, world!", 0, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open..", "Ctrl+O")) { /* Do stuff */ }
            if (ImGui::MenuItem("Save", "Ctrl+S"))   { /* Do stuff */ }
            if (ImGui::MenuItem("Close", "Ctrl+W"))  { }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }


    static int selected = 0;
    ImGui::BeginChild("left pane", ImVec2(300, 0), true);
    for (int i = 0; i < 100; i++)
    {
        char label[128];
        sprintf(label, "MyObject %d", i);
        if (ImGui::Selectable(label, selected == i))
            selected = i;
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // right
    ImGui::BeginGroup();

    ImGui::BeginChild("item view", ImVec2(window_width - 600, 600)); // Leave room for 1 line below us
    if (ImGui::BeginTabBar("##Tabs", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("main.cpp"))
        {

            const char* t = "#include <stdio.h>\n"
                            "int main( int argc, const char* argv[] )\n"
                            "{\n"
                            "    int sum = 0;\n"
                            "    for(int i = 0; i < 10; i++)\n"
                            "    {\n"
                            "    	sum += i;\n"
                            "    }\n"
                            "}\n";

            ImGui::TextWrapped(t);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("lib.h"))
        {
            ImGui::Text("ID: 0123456789");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    // right
    ImGui::BeginChild("log view", ImVec2(window_width - 600, 0)); // Leave room for 1 line below us
    if (ImGui::BeginTabBar("##Tabsasdf", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Console"))
        {
            ImGui::TextWrapped("Console History");
            ImGui::TextWrapped("Console History");
            ImGui::TextWrapped("Console History");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Log"))
        {
            ImGui::TextWrapped("Log Message");
            ImGui::TextWrapped("Log Message");
            ImGui::TextWrapped("Log Message");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginGroup();

    ImGui::BeginChild("right pane", ImVec2(0, 0), true);
    if (ImGui::BeginTabBar("##Tabs2", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Threads"))
        {
            for (int i = 0; i < 4; i++)
            {
                char label[128];
                sprintf(label, "Thread %d", i);
                if (ImGui::Selectable(label, selected == i))
                    selected = i;
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    if (ImGui::BeginTabBar("##Tabs3", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Stack Trace"))
        {
            for (int i = 0; i < 8; i++)
            {
                char label[128];
                sprintf(label, "Func %d", i);
                if (ImGui::Selectable(label, selected == i))
                    selected = i;
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    if (ImGui::BeginTabBar("##Tabs11", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Locals"))
        {
            for (int i = 0; i < 4; i++)
            {
                char label[128];
                sprintf(label, "Local %d", i);
                if (ImGui::Selectable(label, selected == i))
                    selected = i;
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Registers"))
        {
            for (int i = 0; i < 4; i++)
            {
                char label[128];
                sprintf(label, "Registers %d", i);
                if (ImGui::Selectable(label, selected == i))
                    selected = i;
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    if (ImGui::BeginTabBar("##Tabs5", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Watchpoints"))
        {
            for (int i = 0; i < 4; i++)
            {
                char label[128];
                sprintf(label, "Watch %d", i);
                if (ImGui::Selectable(label, selected == i))
                    selected = i;
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Breakpoints"))
        {
            for (int i = 0; i < 4; i++)
            {
                char label[128];
                sprintf(label, "Break %d", i);
                if (ImGui::Selectable(label, selected == i))
                    selected = i;
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::EndGroup();

    ImGui::End();
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

    ret = lldbgui::launch(gui, "/home/zac/lldbg/test/a.out" , const_argv_ptr);
    if (ret != 0) { return EXIT_FAILURE; }

    ret = run_command(gui, "breakpoint set --file simple.cpp --line 5");
    if (ret != 0) { return EXIT_FAILURE; }

    ret = lldbgui::continue_execution(gui);
    if (ret != 0) { return EXIT_FAILURE; }

    ret = lldbgui::process_events(gui);
    if (ret != 0) { return EXIT_FAILURE; }

    // Create GLUT window
    glutInit(&argc, argv);
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_MULTISAMPLE);
    glutInitWindowSize(1280, 720);
    glutCreateWindow("Dear ImGui FreeGLUT+OpenGL2 Example");

    std::cout << "window width: " << glutGet(GLUT_WINDOW_WIDTH) << std::endl;
    std::cout << "window height: " << glutGet(GLUT_WINDOW_HEIGHT) << std::endl;

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

    ImGui::GetStyle().WindowRounding = 0.0f;// <- Set this on init or use ImGui::PushStyleVar()
    ImGui::GetStyle().ChildRounding = 0.0f;
    ImGui::GetStyle().FrameRounding = 0.0f;
    ImGui::GetStyle().GrabRounding = 0.0f;
    ImGui::GetStyle().PopupRounding = 0.0f;
    ImGui::GetStyle().ScrollbarRounding = 0.0f;
    ImGui::GetStyle().TabRounding = 0.0f;


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
