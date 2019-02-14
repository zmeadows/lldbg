#include "Draw.hpp"

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

namespace {
// A convenience struct for extracting pertinent display information from an lldb::SBFrame
struct StackFrameDescription {
    std::string function_name;
    std::string file_name;
    std::string directory;
    int line = -1;
    int column = -1;

    static StackFrameDescription build(lldb::SBFrame frame) {
        StackFrameDescription description;

        auto build_string = [](const char* cstr) -> std::string {
            return cstr ? std::string(cstr) : std::string();
        };

        description.function_name = build_string(frame.GetDisplayFunctionName());
        description.file_name = build_string(frame.GetLineEntry().GetFileSpec().GetFilename());
        description.directory = build_string(frame.GetLineEntry().GetFileSpec().GetDirectory());
        description.directory.append("/"); //FIXME: not cross-platform
        description.line = (int) frame.GetLineEntry().GetLine();
        description.column = (int) frame.GetLineEntry().GetColumn();

        return description;
    }
};

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

bool Splitter(const char* name, bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size)
{
    using namespace ImGui;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiID id = window->GetID(name);
    ImRect bb;
    bb.Min = window->DC.CursorPos + (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
    bb.Max = bb.Min + CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size) : ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
    return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 0.0f);
}

void draw_open_files(lldbg::Application& app) {
    assert(app.open_files.size() > 0);

    optional<size_t> iremove = {};

    for (size_t i = 0; i < app.open_files.size(); i++) {
        const lldbg::FileReference ref = *app.open_files[i];

        const bool is_focused = app.open_files.is_focused(i);

        // we programmatically set the focused tab if manual tab change requested
        // for example when clicking an entry in the stack trace or file explorer
        const auto flags = (app.render_state.request_manual_tab_change && is_focused
                            ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None);

        bool keep_open = true;

        if (ImGui::BeginTabItem(ref.short_name.c_str(), &keep_open, flags)) {
            ImGui::BeginChild("FileContents");
            if (!app.render_state.request_manual_tab_change && !is_focused) {
                // user clicked a tab
                app.open_files.set_focus(i);
                app.text_editor.SetTextLines(*ref.contents);
            }
            app.text_editor.Render("TextEditor");
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (!keep_open) {
            // user clicked close on this tab
            iremove = i;
        }
    }

    if (iremove) {
        app.open_files.close(*iremove);
    }

    app.render_state.request_manual_tab_change = false;
}

void draw_file_browser(lldbg::FileTreeNode* node_to_draw, lldbg::Application& app)
{
    if (!node_to_draw) return;

    if (node_to_draw->is_directory) {
        if (MyTreeNode(node_to_draw->location.c_str())) {
            Defer(ImGui::TreePop());
            node_to_draw->open_children();
            for (const auto& child_node : node_to_draw->children) {
                draw_file_browser(child_node.get(), app);
            }
        }
    } else {
        if (ImGui::Selectable(node_to_draw->location.c_str())) { //, node_to_draw->location.compare(app.render_state.viewed_file) == 0)) {
            const optional<lldbg::FileReference> ref = app.open_files.open(node_to_draw->location);
            if (ref) {
                app.render_state.request_manual_tab_change = true;
                const lldbg::FileReference& ref = *app.open_files.focus();
                app.text_editor.SetTextLines(*ref.contents);
                LOG(Debug) << "Selected " << node_to_draw->location;
            } else {
                LOG(Error) << "Failed to read/open file selected from file browser: " << node_to_draw->location;
            }
        }
    }
}

}

namespace lldbg {

void draw(Application& app)
{
    lldb::SBProcess process = get_process(app);
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
    ImGui::PushFont(app.render_state.font);

    if (ImGui::BeginMenuBar()) {
        Defer(ImGui::EndMenuBar());

        if (ImGui::BeginMenu("File")) {
            Defer(ImGui::EndMenu());
            if (ImGui::MenuItem("Open..", "Ctrl+O")) { /* Do stuff */ }
            if (ImGui::MenuItem("Save", "Ctrl+S"))   { /* Do stuff */ }
            if (ImGui::MenuItem("Close", "Ctrl+W"))  { /* Do stuff */ }
        }

        if (ImGui::BeginMenu("View")) {
            Defer(ImGui::EndMenu());
            if (ImGui::MenuItem("Layout", NULL)) { /* Do stuff */ }
            if (ImGui::MenuItem("Zoom In", "+")) { /* Do stuff */ }
            if (ImGui::MenuItem("Zoom Out", "-")) { /* Do stuff */ }
        }

        if (ImGui::BeginMenu("Help")) {
            Defer(ImGui::EndMenu());
            if (ImGui::MenuItem("About", "F12")) { /* Do stuff */ }
        }
    }

    static float file_browser_width = (float) window_width * app.render_state.DEFAULT_FILEBROWSER_WIDTH_PERCENT;
    static float file_viewer_width = (float) window_width * app.render_state.DEFAULT_FILEVIEWER_WIDTH_PERCENT;
    Splitter("##S1", true, 3.0f, &file_browser_width, &file_viewer_width, 100, 100, window_height);

    if (window_resized) {
        file_browser_width = file_browser_width * (float) new_width / (float) old_width;
        file_viewer_width = file_viewer_width * (float) new_width / (float) old_width;
    }

    ImGui::BeginChild("FileBrowserPane", ImVec2(file_browser_width, 0), true);
    ImGui::TextUnformatted("File Explorer");
    ImGui::Separator();
    draw_file_browser(app.file_browser.get(), app);
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginGroup();

    static float file_viewer_height = window_height/2;
    static float console_height = window_height/2;

    const float old_console_height = console_height;

    Splitter("##S2", false, 3.0f, &file_viewer_height, &console_height, 100, 100, file_viewer_width);

    if (window_resized) {
        file_viewer_height = file_viewer_height * (float) new_height / (float) old_height;
        console_height = console_height * (float) new_height / (float) old_height;
    }

    { // start file viewer
        ImGui::BeginChild("FileViewer", ImVec2(file_viewer_width, file_viewer_height));
        if (ImGui::BeginTabBar("##FileViewerTabs", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_NoTooltip)) {
            Defer(ImGui::EndTabBar());

            if (app.open_files.size() == 0) {
                if (ImGui::BeginTabItem("about")) {
                    Defer(ImGui::EndTabItem());
                    ImGui::TextUnformatted("This is a GUI for lldb.");
                }
            } else {
                draw_open_files(app);
            }

        }
        ImGui::EndChild();
    } // end file viewer

    ImGui::Spacing();

    { // start console/log
        ImGui::BeginChild("LogConsole", ImVec2(file_viewer_width, console_height - 2 * ImGui::GetFrameHeightWithSpacing()));
        if (ImGui::BeginTabBar("##ConsoleLogTabs", ImGuiTabBarFlags_None))
        {
            if (ImGui::BeginTabItem("Console")) {

                ImGui::BeginChild("ConsoleEntries");

                for (const lldbg::CommandLineEntry& entry : app.command_line.get_history()) {
                    ImGui::TextColored(ImVec4(255,0,0,255), "> %s", entry.input.c_str());
                    if (entry.succeeded) {
                        ImGui::TextUnformatted(entry.output.c_str());
                    } else {
                        ImGui::Text("error: %s is not a valid command.", entry.input.c_str());
                    }

                    ImGui::TextUnformatted("\n");
                }

                // always scroll to the bottom of the command history after running a command
                const bool should_auto_scroll_command_window =
                    app.render_state.ran_command_last_frame
                    || old_console_height != console_height;

                auto command_input_callback = [](ImGuiTextEditCallbackData* data) -> int {
                    return 0;
                };

                const ImGuiInputTextFlags command_input_flags =
                    ImGuiInputTextFlags_EnterReturnsTrue;


                static char input_buf[2048];
                if (ImGui::InputText("lldb console", input_buf, 2048, command_input_flags, command_input_callback))
                {
                    app.command_line.run_command(input_buf);
                    strcpy(input_buf, "");
                    app.render_state.ran_command_last_frame = true;
                }

                // Keep auto focus on the input box
                if (ImGui::IsItemHovered() || (ImGui::IsRootWindowOrAnyChildFocused() && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)))
                    ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget

                if (should_auto_scroll_command_window) {
                    ImGui::SetScrollHere(1.0f);
                    app.render_state.ran_command_last_frame = false;
                }

                ImGui::EndChild();

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Log")) {
                ImGui::BeginChild("LogEntries");
                lldbg::g_logger.for_each_message([](const lldbg::LogMessage& message) -> void {
                    //TODO: colorize based on log level
                    ImGui::TextUnformatted(message.message.c_str());
                });
                ImGui::SetScrollHere(1.0f);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::EndChild();
    } // end console/log

    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginGroup();

    const float threads_height    = window_height/4;
    const float stack_height      = window_height/4;
    const float locals_height     = window_height/4;
    const float breakpoint_height = window_height/4;

    ImGui::BeginChild("#ThreadsChild", ImVec2(window_width - file_browser_width - file_viewer_width, threads_height), true);
    if (ImGui::BeginTabBar("#ThreadsTabs", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Threads"))
        {
            if (stopped)
            {
                for (uint32_t i = 0; i < process.GetNumThreads(); i++) {
                    char label[128];
                    sprintf(label, "Thread %d", i);
                    if (ImGui::Selectable(label, i == app.render_state.viewed_thread_index)) {
                        app.render_state.viewed_thread_index = i;
                    }
                }

                if (process.GetNumThreads() > 0 && app.render_state.viewed_thread_index < 0) {
                    app.render_state.viewed_thread_index = 0;
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

            if (stopped && app.render_state.viewed_thread_index >= 0) {
                ImGui::Columns(3);
                ImGui::Separator();
                ImGui::Text("FUNCTION");
                ImGui::NextColumn();
                ImGui::Text("FILE");
                ImGui::NextColumn();
                ImGui::Text("LINE");
                ImGui::NextColumn();
                ImGui::Separator();

                lldb::SBThread viewed_thread = process.GetThreadAtIndex(app.render_state.viewed_thread_index);
                for (uint32_t i = 0; i < viewed_thread.GetNumFrames(); i++) {
                    const auto desc = StackFrameDescription::build(viewed_thread.GetFrameAtIndex(i));

                    if (ImGui::Selectable(desc.function_name.c_str() ? desc.function_name.c_str() : "unknown", (int) i == selected_row)) {
                        const std::string full_path = desc.directory + desc.file_name;
                        app.open_files.open(full_path);
                        app.render_state.request_manual_tab_change = true;
                        const lldbg::FileReference& ref = *app.open_files.focus();
                        app.text_editor.SetTextLines(*ref.contents);
                        selected_row = (int) i;
                    }
                    ImGui::NextColumn();

                    ImGui::Selectable(desc.file_name.c_str() ? desc.file_name.c_str() : "unknown", (int) i == selected_row);
                    ImGui::NextColumn();

                    static char line_buf[256];
                    sprintf(line_buf, "%d", desc.line);
                    ImGui::Selectable(line_buf, (int) i == selected_row);
                    ImGui::NextColumn();
                }

                app.render_state.viewed_frame_index = selected_row;
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
            //TODO: turn this into a recursive tree that displays children of structs/arrays
            if (stopped && app.render_state.viewed_frame_index >= 0) {
                lldb::SBThread viewed_thread = process.GetThreadAtIndex(app.render_state.viewed_thread_index);
                lldb::SBFrame frame = viewed_thread.GetFrameAtIndex(app.render_state.viewed_frame_index);
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
    if (ImGui::BeginTabBar("##BreakWatchPointTabs", ImGuiTabBarFlags_None)) {
        Defer(ImGui::EndTabBar());

        if (ImGui::BeginTabItem("Watchpoints")) {
            Defer(ImGui::EndTabItem());

            for (int i = 0; i < 4; i++)
            {
                char label[128];
                sprintf(label, "Watch %d", i);
                if (ImGui::Selectable(label, i == 0)) {
                    // blah
                }
            }
        }

        if (ImGui::BeginTabItem("Breakpoints")) {
            Defer(ImGui::EndTabItem());

            if (stopped && app.render_state.viewed_thread_index >= 0) {

                static int selected_row = -1;

                ImGui::Columns(2);
                ImGui::Separator();
                ImGui::Text("FILE");
                ImGui::NextColumn();
                ImGui::Text("LINE");
                ImGui::NextColumn();
                ImGui::Separator();

                lldb::SBTarget target = app.debugger.GetSelectedTarget();
                for (uint32_t i = 0; i < target.GetNumBreakpoints(); i++)
                {

                    lldb::SBBreakpoint breakpoint = target.GetBreakpointAtIndex(i);
                    lldb::SBBreakpointLocation location = breakpoint.FindLocationByID(breakpoint.GetID());

                    if (!location.IsValid()) {
                        LOG(Error) << "Invalid breakpoint location encountered!";
                    }

                    lldb::SBAddress address = location.GetAddress();

                    if (!address.IsValid()) {
                        LOG(Error) << "Invalid lldb::SBAddress for breakpoint!";
                    }

                    lldb::SBLineEntry line_entry = address.GetLineEntry();

                    const char* filename = line_entry.GetFileSpec().GetFilename();
                    ImGui::Selectable(filename, (int) i == selected_row);
                    ImGui::NextColumn();

                    int line_number = line_entry.GetLine();

                    static char line_buf[256];
                    sprintf(line_buf, "%d", line_number);
                    ImGui::Selectable(line_buf, (int) i == selected_row);
                    ImGui::NextColumn();
                }

                ImGui::Columns(1);
            }
        }
    }
    ImGui::EndChild();

    ImGui::EndGroup();

    ImGui::PopFont();
    ImGui::End();
}

}
