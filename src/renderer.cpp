#include "renderer.hpp"

#include "Log.hpp"

#include <stdio.h>
#include <string>


#include "imgui.h"
#include "examples/imgui_impl_freeglut.h"
#include "examples/imgui_impl_opengl2.h"
#include <GL/freeglut.h>

namespace lldbg {

void draw(lldb::SBProcess process)
{
    static float f = 0.0f;
    static int counter = 0;

    //TODO: update this with a callback instead of grabbing every frame
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

            std::string program = "#include <stdio.h>\n"
                            "int main( int argc, const char* argv[] )\n"
                            "{\n"
                            "    int sum = 0;\n"
                            "    for(int i = 0; i < 10; i++)\n"
                            "    {\n"
                            "    	sum += i;\n"
                            "    }\n"
                            "}\n";

            ImGui::TextWrapped(program.c_str());
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
        if (ImGui::BeginTabItem("Log"))
        {
            lldbg::g_logger.for_each_message([](const lldbg::LogMessage& message) -> void {
                ImGui::TextWrapped(message.message.c_str());
            });
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Console"))
        {
            ImGui::TextWrapped("Console History");
            ImGui::TextWrapped("Console History");
            ImGui::TextWrapped("Console History");
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

}
