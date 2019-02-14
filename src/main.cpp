#include <iostream>

#include "lldb/API/LLDB.h"

#include "Application.hpp"
#include "Defer.hpp"
#include "Log.hpp"
#include "Draw.hpp"
#include "FileSystem.hpp"
#include "Timer.hpp"

#include <GL/freeglut.h>
#include "imgui.h"
#include "examples/imgui_impl_freeglut.h"
#include "examples/imgui_impl_opengl2.h"

#include <vector>
#include <string>
#include <fstream>

void tick(lldbg::Application& app) {
    lldb::SBEvent event;

    while (true) {
        optional<lldb::SBEvent> event = app.event_listener.pop_event();

        if (event) {
            const lldb::StateType new_state = lldb::SBProcess::GetStateFromEvent(*event);
            const char* state_descr = lldb::SBDebugger::StateAsCString(new_state);
            LOG(Debug) << "Found event with new state: " << state_descr;
        } else {
            break;
        }
    }

    lldbg::draw(app);
}


void main_loop() {
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplFreeGLUT_NewFrame();


    tick(lldbg::g_application);

    // Rendering
    ImGui::Render();
    ImGuiIO& io = ImGui::GetIO();
    glViewport(0, 0, (GLsizei)io.DisplaySize.x, (GLsizei)io.DisplaySize.y);
    // glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

    glutSwapBuffers();
    glutPostRedisplay();
}

void initialize_rendering(int* argcp, char** argv) {
    // Create GLUT window
    glutInit(argcp, argv);
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_MULTISAMPLE);
    glutInitWindowSize(1280, 720);
    glutCreateWindow("lldbg");

    // Setup GLUT display function
    // We will also call ImGui_ImplFreeGLUT_InstallFuncs() to get all the other functions installed for us,
    // otherwise it is possible to install our own functions and call the imgui_impl_freeglut.h functions ourselves.
    glutDisplayFunc(main_loop);

    // Setup Dear ImGui context
    ImGui::CreateContext();
    // ImGuiIO& io = ImGui::GetIO();
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // disable all rounding
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

}

void cleanup_rendering() {
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplFreeGLUT_Shutdown();
    ImGui::DestroyContext();
}

namespace lldbg {
Logger g_logger;
Application g_application;
}

int main(int argc, char** argv)
{
    lldb::SBDebugger::Initialize();
    initialize_rendering(&argc, argv);

    Defer(
        lldb::SBDebugger::Terminate();
        cleanup_rendering();
        );

    std::vector<std::string> args( argv + 1, argv + argc );
    std::vector<const char*> const_argv;

    for (const std::string& arg : args) {
        const_argv.push_back(arg.c_str());
    }

    const char** const_argv_ptr = const_argv.data();

    lldbg::start_process(lldbg::g_application, "/home/zac/lldbg/test/a.out", const_argv_ptr);

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();
    //TODO: read font path from CMake-defined variable
    lldbg::g_application.render_state.font = io.Fonts->AddFontFromFileTTF("../lib/imgui/misc/fonts/Hack-Regular.ttf", 15.0f);


    glutMainLoop();

    return 0;
}
