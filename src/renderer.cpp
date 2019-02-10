#include "renderer.hpp"

#include "Log.hpp"
#include "CustomTreeNode.hpp"

#include <stdio.h>
#include <string>


#include "imgui.h"
#include <imgui_internal.h>
#include "examples/imgui_impl_freeglut.h"
#include "examples/imgui_impl_opengl2.h"
#include <GL/freeglut.h>

namespace {

bool MyTreeNode(const char* label)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;

    ImGuiID id = window->GetID(label);
    ImVec2 pos = window->DC.CursorPos;
    ImRect bb(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x, pos.y + g.FontSize + g.Style.FramePadding.y*2));
    bool opened = ImGui::TreeNodeBehaviorIsOpen(id);
    bool hovered, held;
    if (ImGui::ButtonBehavior(bb, id, &hovered, &held, true))
        window->DC.StateStorage->SetInt(id, opened ? 0 : 1);
    if (hovered || held)
        window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(held ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered));

    // Icon, text
    float button_sz = g.FontSize + g.Style.FramePadding.y*2;
    //TODO: set colors from style?
    window->DrawList->AddRectFilled(pos, ImVec2(pos.x+button_sz, pos.y+button_sz), opened ? ImColor(51,105,173) : ImColor(42,79,130));
    ImGui::RenderText(ImVec2(pos.x + button_sz + g.Style.ItemInnerSpacing.x, pos.y + g.Style.FramePadding.y), label);

    ImGui::ItemSize(bb, g.Style.FramePadding.y);
    ImGui::ItemAdd(bb, id);

    if (opened)
        ImGui::TreePush(label);
    return opened;
}

bool Splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.f)
{
    using namespace ImGui;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiID id = window->GetID("##Splitter");
    ImRect bb;
    bb.Min = window->DC.CursorPos + (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
    bb.Max = bb.Min + CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size) : ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
    return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 0.0f);
}

void draw(FileTreeNode* node) {
    if (!node) return;
    if (node->is_directory) {
        if (MyTreeNode(node->location.c_str())) {
            node->open_children();
            for (const auto& child_node : node->children) {
                draw(child_node.get());
            }
            ImGui::TreePop();
        }
    } else {
        ImGui::TextUnformatted(node->location.c_str());
    }
}


}

namespace lldbg {


void draw(lldb::SBProcess process, LLDBCommandLine& command_line, FileBrowser& fs, RenderState& state)
{
    const bool stopped = process.GetState() == lldb::eStateStopped;

    // ImGuiIO& io = ImGui::GetIO();
    // io.FontGlobalScale = 1.1;

    static int window_width = glutGet(GLUT_WINDOW_WIDTH);
    static int window_height = glutGet(GLUT_WINDOW_HEIGHT);

    bool window_resized = false;
    const int old_width = window_width;
    const int old_height = window_height;
    const int new_width = glutGet(GLUT_WINDOW_WIDTH);
    const int new_height = glutGet(GLUT_WINDOW_HEIGHT);
    if (new_width != old_width || new_height != old_height) {
        window_width = new_width;
        window_height = new_height;
        window_resized = true;
    }

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

        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Layout", NULL)) { /* Do stuff */ }
            if (ImGui::MenuItem("Zoom In", "+")) { /* Do stuff */ }
            if (ImGui::MenuItem("Zoom Out", "-")) { /* Do stuff */ }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("About", "F12")) { /* Do stuff */ }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGui::BeginChild("FileBrowserPane", ImVec2(300, 0), true);
    ImGui::TextUnformatted("File Explorer");
    ImGui::Separator();

    draw(fs.base_node());

    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginGroup();

    static float file_viewer_height = window_height/2;
    static float console_height = window_height/2;
    Splitter(false, 5.0f, &file_viewer_height, &console_height, 100, 100, window_width-900);

    if (window_resized) {
        file_viewer_height = file_viewer_height * (float) new_height / (float) old_height;
        console_height = console_height * (float) new_height / (float) old_height;
    }

    ImGui::BeginChild("FileViewer", ImVec2(window_width - 900, file_viewer_height));
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

            ImGui::TextUnformatted(program.c_str());
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

    ImGui::Spacing();

    ImGui::BeginChild("LogConsole", ImVec2(window_width - 900, console_height - 2 * ImGui::GetFrameHeightWithSpacing()));
    if (ImGui::BeginTabBar("##ConsoleLogTabs", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Console"))
        {
            for (const CommandLineEntry& entry : command_line.get_history()) {
                ImGui::TextColored(ImVec4(255,0,0,255), "> %s", entry.input.c_str());
                if (entry.succeeded) {
                    ImGui::TextUnformatted(entry.output.c_str());
                } else {
                    ImGui::Text("error: %s is not a valid command.", entry.input.c_str());
                }

                ImGui::TextUnformatted("\n");
            }
            static char input_buf[1024];

            auto command_input_callback = [](ImGuiTextEditCallbackData* data) -> int {
                return 0;
            };

            const ImGuiInputTextFlags command_input_flags =
                ImGuiInputTextFlags_EnterReturnsTrue
                | ImGuiInputTextFlags_CallbackCompletion
                | ImGuiInputTextFlags_CallbackHistory;


            if (ImGui::InputText("COMMAND:", input_buf, 1024, command_input_flags, command_input_callback))
            {
                command_line.run_command(input_buf);
                strcpy(input_buf, "");
            }

            // always scroll to the bottom of the command history
            ImGui::SetScrollHere(1.0f);

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Log"))
        {
            lldbg::g_logger.for_each_message([](const lldbg::LogMessage& message) -> void {
                ImGui::TextUnformatted(message.message.c_str());
            });
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }


    ImGui::EndChild();

    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginGroup();

    const float threads_height    = window_height/4;
    const float stack_height      = window_height/4;
    const float locals_height     = window_height/4;
    const float breakpoint_height = window_height/4;

    ImGui::BeginChild("#ThreadsChild", ImVec2(0, threads_height), true);
    if (ImGui::BeginTabBar("#ThreadsTabs", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Threads"))
        {
            if (stopped) {
                for (uint32_t i = 0; i < process.GetNumThreads(); i++) {
                    char label[128];
                    sprintf(label, "Thread %d", i);
                    if (ImGui::Selectable(label, i == state.viewed_thread_index)) {
                        state.viewed_thread_index = i;
                    }
                }

                if (process.GetNumThreads() > 0 && state.viewed_thread_index < 0) {
                    state.viewed_thread_index = 0;
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    ImGui::BeginChild("#StackTraceChild", ImVec2(0, stack_height), true);
    if (ImGui::BeginTabBar("##StackTraceTabs", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Stack Trace"))
        {

            static int selected_row = -1;

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

                lldb::SBThread viewed_thread = process.GetThreadAtIndex(state.viewed_thread_index);
                for (uint32_t i = 0; i < viewed_thread.GetNumFrames(); i++) {
                    auto desc = StackFrameDescription::build(viewed_thread.GetFrameAtIndex(i));

                    if (ImGui::Selectable(desc.function_name ? desc.function_name : "unknown", (int) i == selected_row)) {
                        selected_row = (int) i;
                    }
                    ImGui::NextColumn();

                    if (ImGui::Selectable(desc.file_name ? desc.function_name : "unknown", (int) i == selected_row)) {
                        selected_row = (int) i;
                    }
                    ImGui::NextColumn();

                    char line_buf[32];
                    sprintf(line_buf, "%d", desc.line);
                    if (ImGui::Selectable(line_buf, (int) i == selected_row)) {
                        selected_row = (int) i;
                    }
                    ImGui::NextColumn();

                    char column_buf[32];
                    sprintf(column_buf, "%d", desc.column);
                    if (ImGui::Selectable(column_buf, (int) i == selected_row)) {
                        selected_row = (int) i;
                    }
                    ImGui::NextColumn();
                }

                state.viewed_frame_index = selected_row;
                ImGui::Columns(1);
            }

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    ImGui::BeginChild("#LocalsChild", ImVec2(0, locals_height), true);
    if (ImGui::BeginTabBar("##LocalsTabs", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Locals"))
        {
            if (stopped && state.viewed_frame_index >= 0) {
                lldb::SBThread viewed_thread = process.GetThreadAtIndex(state.viewed_thread_index);
                lldb::SBFrame frame = viewed_thread.GetFrameAtIndex(state.viewed_frame_index);
                lldb::SBValueList locals = frame.GetVariables(true, true, true, true);
                for (uint32_t i = 0; i < locals.GetSize(); i++) {
                    lldb::SBValue value = locals.GetValueAtIndex(i);
                    ImGui::TextUnformatted(value.GetName());
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
    ImGui::EndChild();

    ImGui::BeginChild("#BreakWatchPointChild", ImVec2(0, breakpoint_height), true);
    if (ImGui::BeginTabBar("##BreakWatchPointTabs", ImGuiTabBarFlags_None))
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
    ImGui::EndGroup();

    ImGui::End();
}

}
