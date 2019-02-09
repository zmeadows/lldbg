#include "renderer.hpp"

#include "Log.hpp"
#include "CustomTreeNode.hpp"

#include <stdio.h>
#include <string>


#include "imgui.h"
#include "examples/imgui_impl_freeglut.h"
#include "examples/imgui_impl_opengl2.h"
#include <GL/freeglut.h>

namespace lldbg {

void draw(lldb::SBProcess process, RenderState& state)
{
    const bool stopped = process.GetState() == lldb::eStateStopped;

    //TODO: update this with a callback instead of grabbing every frame
    const int window_width = glutGet(GLUT_WINDOW_WIDTH);
    const int window_height = glutGet(GLUT_WINDOW_HEIGHT);

    ImGui::SetNextWindowPos(ImVec2(0.f,0.f), ImGuiSetCond_Always);
    ImGui::SetNextWindowSize(ImVec2(window_width, window_height), ImGuiSetCond_Always);
    ImGui::Begin("lldbg", 0, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open..", "Ctrl+O")) { /* Do stuff */ }
            if (ImGui::MenuItem("Save", "Ctrl+S"))   { /* Do stuff */ }
            if (ImGui::MenuItem("Close", "Ctrl+W"))  { /* Do stuff */ }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGui::BeginChild("FileBrowserPane", ImVec2(300, 0), true);
    // if (ImGui::Button("CWD")) { }

    if (MyTreeNode("dir1")) {
        if (MyTreeNode("dir2")) {
            ImGui::Text("file1");
            ImGui::TreePop();
        }
        ImGui::Text("file2");
        ImGui::Text("file3");
        ImGui::TreePop();
    }

    if (MyTreeNode("dir3")) {
        ImGui::Text("file4");
        ImGui::Text("file5");
        ImGui::TreePop();
    }

    ImGui::EndChild();
    ImGui::SameLine();

    // right
    ImGui::BeginGroup();

    ImGui::BeginChild("FileViewer", ImVec2(window_width - 900, 600)); // Leave room for 1 line below us
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
        //TODO: what happens when the TabBar is filled?
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    // right
    ImGui::BeginChild("LogConsole", ImVec2(window_width - 900, 0));
    if (ImGui::BeginTabBar("##ConsoleLogTabs", ImGuiTabBarFlags_None))
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

    ImGui::BeginChild("Trace", ImVec2(0, 0), true);
    if (ImGui::BeginTabBar("##ThreadTabs", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Threads"))
        {
            if (stopped) {
                for (uint32_t i = 0; i < process.GetNumThreads(); i++) {
                    char label[128];
                    sprintf(label, "Thread %d", i);
                    if (ImGui::Selectable(label, i == state.viewed_thread_index)) {
                        state.viewed_thread_index = i;
                        lldb::SBThread viewed_thread = process.GetThreadAtIndex(i);
                        update_stack_trace(viewed_thread, state.stack_trace);
                    }
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    if (ImGui::BeginTabBar("##StackTraceTabs", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Stack Trace"))
        {

            if (stopped && state.viewed_thread_index >= 0) {
                ImGui::Columns(4);
                ImGui::Separator();
                ImGui::Text("FUNCTION");
                ImGui::NextColumn();
                ImGui::Text("FILE");
                ImGui::NextColumn();
                ImGui::Text("LINE");
                ImGui::NextColumn();
                ImGui::Text("COLUMN");
                ImGui::NextColumn();
                ImGui::Separator();

                for (const StackTraceEntry& entry : state.stack_trace) {
                    ImGui::Text(entry.function_name.c_str());
                    ImGui::NextColumn();
                    ImGui::Text(entry.file_name.c_str());
                    ImGui::NextColumn();
                    ImGui::Text(entry.line.c_str());
                    ImGui::NextColumn();
                    ImGui::Text(entry.column.c_str());
                    ImGui::NextColumn();
                }
                ImGui::Columns(1);
            }

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    if (ImGui::BeginTabBar("##Tabs11", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Locals"))
        {
            if (stopped && state.viewed_frame_index >= 0) {
                lldb::SBThread viewed_thread = process.GetThreadAtIndex(state.viewed_thread_index);
                lldb::SBFrame frame = viewed_thread.GetFrameAtIndex(state.viewed_frame_index);
                lldb::SBValueList locals = frame.GetVariables(true, true, true, true);
                for (uint32_t i = 0; i < locals.GetSize(); i++) {
                    lldb::SBValue value = locals.GetValueAtIndex(i);
                    ImGui::Text(value.GetName());
                }
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Registers"))
        {
            for (int i = 0; i < 4; i++)
            {
                char label[128];
                sprintf(label, "Registers %d", i);
                if (ImGui::Selectable(label, i == 0)) {
                    // blah
                }
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
                if (ImGui::Selectable(label, i == 0)) {
                    // blah
                }
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Breakpoints"))
        {
            for (int i = 0; i < 4; i++)
            {
                char label[128];
                sprintf(label, "Break %d", i);
                if (ImGui::Selectable(label, i == 0)) {
                    // blah
                }
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
