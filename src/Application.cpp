#include "Application.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <map>
#include <queue>
#include <thread>
#include <unordered_set>

#include "Defer.hpp"
#include "Log.hpp"
#include "StringBuffer.hpp"
#include "fmt/format.h"

namespace fs = std::filesystem;

using namespace lldbg;

static std::map<std::string, std::string> s_debug_stream;

// NOTE: remember this is updated once per frame and currently variables are never removed
#define DEBUG_STREAM(x)                                \
    {                                                  \
        const std::string xkey = std::string(#x);      \
        auto it = s_debug_stream.find(xkey);           \
        const std::string xstr = fmt::format("{}", x); \
        if (it != s_debug_stream.end()) {              \
            it->second = xstr;                         \
        }                                              \
        else {                                         \
            s_debug_stream[xkey] = xstr;               \
        }                                              \
    }

/*
static void continue_process(Application& app)
{
    lldb::SBProcess process = app.debugger.GetSelectedTarget().GetProcess();
    assert(process.IsValid());
    process.Continue();
}

static void pause_process(Application& app)
{
    lldb::SBProcess process = app.debugger.GetSelectedTarget().GetProcess();
    assert(process.IsValid());
    process.Stop();
}

static void kill_process(Application& app)
{
    lldb::SBProcess process = app.debugger.GetSelectedTarget().GetProcess();
    assert(process.IsValid());
    process.Kill();
}

static void delete_current_targets(Application& app)
{
    lldb::SBDebugger& dbg = app.debugger;
    for (uint32_t i = 0; i < dbg.GetNumTargets(); i++) {
        lldb::SBTarget target = dbg.GetTargetAtIndex(i);
        dbg.DeleteTarget(target);
    }
}
*/

static void add_breakpoint_to_viewed_file(Application& app, int line)
{
    if (!app.open_files.focus()) {
        LOG(Warning) << "add_breakpoint_to_viewed_file called while no file was focused";
        return;
    }

    FileHandle focus = *app.open_files.focus();

    const std::string& filepath = focus.filepath();
    lldb::SBTarget target = app.debugger.GetSelectedTarget();

    lldb::SBBreakpoint new_breakpoint =
        target.BreakpointCreateByLocation(filepath.c_str(), line);

    auto undo_breakpoint = [&](const char* msg) -> void {
        LOG(Warning) << "Failed to add breakpoint. Reason: " << msg;
        target.BreakpointDelete(new_breakpoint.GetID());
    };

    if (!new_breakpoint.IsValid() || new_breakpoint.GetNumLocations() == 0) {
        undo_breakpoint("no locations found for specified breakpoint");
        return;
    }

    lldb::SBBreakpointLocation location = new_breakpoint.GetLocationAtIndex(0);
    if (!location.IsValid() || !location.IsEnabled()) {
        undo_breakpoint("breakpoint locations found, but they are invalid");
        return;
    }

    lldb::SBAddress address = location.GetAddress();
    if (!address.IsValid()) {
        undo_breakpoint("Invalid breakpoint address");
        return;
    }

    const std::unordered_set<int>* bps = app.breakpoints.Get(focus);
    if (bps != nullptr && bps->find(address.GetLineEntry().GetLine()) != bps->end()) {
        undo_breakpoint("duplicate");
        return;
    }

    app.breakpoints.synchronize(app.debugger.GetSelectedTarget());
    app.text_editor.SetBreakpoints(app.breakpoints.Get(focus));
    LOG(Verbose) << "Successfully added breakpoint in file " << filepath
                 << " at line: " << line;
}

static std::string build_string(const char* cstr)
{
    return cstr ? std::string(cstr) : std::string();
}

static inline bool has_target(Application& app)
{
    const uint32_t target_count = app.debugger.GetNumTargets();
    assert(target_count == 0 || target_count == 1);
    return target_count > 0;
}

static lldb::SBProcess get_process(Application& app)
{
    assert(has_target(app));
    return app.debugger.GetSelectedTarget().GetProcess();
}

static void glfw_error_callback(int error, const char* description)
{
    StringBuffer buffer;
    buffer.format("GLFW Error {}: {}\n", error, description);
    LOG(Error) << buffer.data();
}

// A convenience struct for extracting pertinent display information from an lldb::SBFrame
struct StackFrame {
    FileHandle file_handle;
    std::string function_name;
    int line;
    int column;

private:
    StackFrame(FileHandle _file_handle, int _line, int _column, std::string&& _function_name)
        : file_handle(_file_handle),
          function_name(_function_name),
          line(_line),
          column(_column)
    {
    }

public:
    static std::optional<StackFrame> create(lldb::SBFrame frame)
    {
        lldb::SBFileSpec spec = frame.GetLineEntry().GetFileSpec();
        fs::path filename = fs::path(build_string(spec.GetFilename()));
        fs::path directory = fs::path(build_string(spec.GetDirectory()));

        if (!fs::exists(directory)) return {};

        if (auto handle = FileHandle::create(directory / filename); handle.has_value()) {
            return StackFrame(*handle, (int)frame.GetLineEntry().GetLine(),
                              (int)frame.GetLineEntry().GetColumn(),
                              build_string(frame.GetDisplayFunctionName()));
        }
        else {
            return {};
        }
    }
};

static bool FileTreeNode(const char* label)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;

    ImGuiID id = window->GetID(label);
    ImVec2 pos = window->DC.CursorPos;
    ImRect bb(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x,
                          pos.y + g.FontSize + g.Style.FramePadding.y * 2));

    bool opened = ImGui::TreeNodeBehaviorIsOpen(id);
    bool hovered, held;

    if (ImGui::ButtonBehavior(bb, id, &hovered, &held, true))
        window->DC.StateStorage->SetInt(id, opened ? 0 : 1);

    if (hovered || held)
        window->DrawList->AddRectFilled(
            bb.Min, bb.Max,
            ImGui::GetColorU32(held ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered));

    // Icon, text
    float button_sz = g.FontSize + g.Style.FramePadding.y * 2;
    // TODO: set colors from style?
    window->DrawList->AddRectFilled(pos, ImVec2(pos.x + button_sz, pos.y + button_sz),
                                    opened ? ImColor(51, 105, 173) : ImColor(42, 79, 130));

    const auto label_location =
        ImVec2(pos.x + button_sz + g.Style.ItemInnerSpacing.x, pos.y + g.Style.FramePadding.y);

    ImGui::RenderText(label_location, label);

    ImGui::ItemSize(bb, g.Style.FramePadding.y);
    ImGui::ItemAdd(bb, id);

    if (opened) ImGui::TreePush(label);

    return opened;
}

static bool Splitter(const char* name, bool split_vertically, float thickness, float* size1,
                     float* size2, float min_size1, float min_size2,
                     float splitter_long_axis_size)
{
    using namespace ImGui;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiID id = window->GetID(name);
    ImRect bb;
    bb.Min = window->DC.CursorPos +
             (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
    bb.Max =
        bb.Min + CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size)
                                               : ImVec2(splitter_long_axis_size, thickness),
                              0.0f, 0.0f);
    return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2,
                            min_size1, min_size2, 0.0f);
}

static void draw_open_files(lldbg::Application& app)
{
    bool closed_tab = false;

    app.open_files.for_each_open_file([&](FileHandle handle, bool is_focused) {
        auto action = lldbg::OpenFiles::Action::Nothing;

        // we programmatically set the focused tab if manual tab change requested
        // for example when the user clicks an entry in the stack trace or file explorer
        auto tab_flags = ImGuiTabItemFlags_None;
        if (app.ui.request_manual_tab_change && is_focused) {
            tab_flags = ImGuiTabItemFlags_SetSelected;
            app.text_editor.SetTextLines(handle.contents());
            app.text_editor.SetBreakpoints(app.breakpoints.Get(handle));
        }

        bool keep_tab_open = true;
        if (ImGui::BeginTabItem(handle.filename().c_str(), &keep_tab_open, tab_flags)) {
            ImGui::BeginChild("FileContents");
            if (!app.ui.request_manual_tab_change && !is_focused) {
                // user selected tab directly with mouse
                action = lldbg::OpenFiles::Action::ChangeFocusTo;
                app.text_editor.SetTextLines(handle.contents());
                app.text_editor.SetBreakpoints(app.breakpoints.Get(handle));
            }
            app.text_editor.Render("TextEditor");
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (!keep_tab_open) {  // user closed tab with mouse
            closed_tab = true;
            action = lldbg::OpenFiles::Action::Close;
        }

        return action;
    });

    app.ui.request_manual_tab_change = false;

    if (closed_tab && app.open_files.size() > 0) {
        FileHandle handle = *app.open_files.focus();
        app.text_editor.SetTextLines(handle.contents());
        app.text_editor.SetBreakpoints(app.breakpoints.Get(handle));
    }
}

static void manually_open_and_or_focus_file(lldbg::Application& app, const char* filepath)
{
    // TODO: check if attempting to open currently focused file and do nothing if so
    // to avoid unnecessarily re-loading/drawing lines in text editor
    // TODO: call manually_open_and_or_focus_file with handle arg here instead
    if (app.open_files.open(std::string(filepath))) {
        app.ui.request_manual_tab_change = true;
    }
}

static void manually_open_and_or_focus_file(lldbg::Application& app, FileHandle handle)
{
    app.open_files.open(handle);
    app.ui.request_manual_tab_change = true;
}

static void draw_file_browser(lldbg::Application& app, lldbg::FileBrowserNode* node_to_draw,
                              size_t depth)
{
    if (node_to_draw->is_directory()) {
        const char* tree_node_label =
            depth == 0 ? node_to_draw->filepath() : node_to_draw->filename();

        if (FileTreeNode(tree_node_label)) {
            for (auto& child_node : node_to_draw->children()) {
                draw_file_browser(app, child_node.get(), depth + 1);
            }
            ImGui::TreePop();
        }
    }
    else {
        if (ImGui::Selectable(node_to_draw->filename())) {
            manually_open_and_or_focus_file(app, node_to_draw->filepath());
        }
    }
}

static bool run_lldb_command(Application& app, const char* command)
{
    const size_t num_breakpoints_before = app.debugger.GetSelectedTarget().GetNumBreakpoints();

    const bool command_succeeded = app.command_line.run_command(command);

    const size_t num_breakpoints_after = app.debugger.GetSelectedTarget().GetNumBreakpoints();

    if (num_breakpoints_before != num_breakpoints_after) {
        // TODO: just always synchronize, since it is cheap and user commands won't be run very
        // often?
        app.breakpoints.synchronize(app.debugger.GetSelectedTarget());

        const std::optional<FileHandle> maybe_handle = app.open_files.focus();

        if (maybe_handle) {
            auto handle = *maybe_handle;
            app.text_editor.SetTextLines(handle.contents());
            app.text_editor.SetBreakpoints(app.breakpoints.Get(handle));
        }
    }

    return command_succeeded;
}

namespace lldbg {

void draw(Application& app)
{
    lldb::SBProcess process = get_process(app);
    const bool stopped = process.GetState() == lldb::eStateStopped;

    // ImGuiIO& io = ImGui::GetIO();
    // io.FontGlobalScale = 1.1;

    // TODO: this window height/width tracking is unnecessarily complicated
    // TODO: detect if window has been made very small, and just display a warning message
    // instead of trying to draw the layout
    static int window_width = app.ui.window_width;
    static int window_height = app.ui.window_height;

    bool window_resized = false;
    const int old_width = window_width;
    const int old_height = window_height;
    const int new_width = app.ui.window_width;
    const int new_height = app.ui.window_height;

    if (new_width != old_width || new_height != old_height) {
        window_width = new_width;
        window_height = new_height;
        window_resized = true;
    }

    DEBUG_STREAM(window_width);
    DEBUG_STREAM(window_height);

    const char* process_state = lldb::SBDebugger::StateAsCString(process.GetState());
    assert(process_state);
    DEBUG_STREAM(process_state);

    const auto window_width_f = static_cast<float>(window_width);
    const auto window_height_f = static_cast<float>(window_height);

    ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(window_width_f, window_height_f), ImGuiCond_Always);

    ImGui::Begin("lldbg", 0,
                 ImGuiWindowFlags_NoBringToFrontOnFocus |  // ImGuiWindowFlags_MenuBar |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoTitleBar);
    ImGui::PushFont(app.ui.font);

    // if (ImGui::BeginMenuBar()) {
    //     Defer(ImGui::EndMenuBar());

    //     if (ImGui::BeginMenu("File")) {
    //         Defer(ImGui::EndMenu());
    //         if (ImGui::MenuItem("Open..", "Ctrl+O")) { /* Do stuff */
    //         }
    //         if (ImGui::MenuItem("Save", "Ctrl+S")) { /* Do stuff */
    //         }
    //         if (ImGui::MenuItem("Close", "Ctrl+W")) { /* Do stuff */
    //         }
    //     }

    //     if (ImGui::BeginMenu("View")) {
    //         Defer(ImGui::EndMenu());
    //         if (ImGui::MenuItem("Layout", NULL)) { /* Do stuff */
    //         }
    //         if (ImGui::MenuItem("Zoom In", "+")) { /* Do stuff */
    //         }
    //         if (ImGui::MenuItem("Zoom Out", "-")) { /* Do stuff */
    //         }
    //     }

    //     if (ImGui::BeginMenu("Help")) {
    //         Defer(ImGui::EndMenu());
    //         if (ImGui::MenuItem("About", "F12")) { /* Do stuff */
    //         }
    //     }
    // }

    static float file_browser_width =
        window_width_f * app.ui.DEFAULT_FILEBROWSER_WIDTH_PERCENT;
    static float file_viewer_width = window_width_f * app.ui.DEFAULT_FILEVIEWER_WIDTH_PERCENT;
    Splitter("##S1", true, 3.0f, &file_browser_width, &file_viewer_width, 100, 100,
             window_height_f);

    if (window_resized) {
        file_browser_width = file_browser_width * (float)new_width / (float)old_width;
        file_viewer_width = file_viewer_width * (float)new_width / (float)old_width;
    }

    ImGui::BeginChild("FileBrowserPane", ImVec2(file_browser_width, 0));

    if (has_target(app)) {
        // TODO: show rightmost chunk of path in case it is too long to fit on screen
        // TODO: add button to select new target using
        // https://github.com/aiekick/ImGuiFileDialog
        lldb::SBFileSpec fs = app.debugger.GetSelectedTarget().GetExecutable();

        StringBuffer target_description;

        const char* target_directory = fs.GetDirectory();
        const char* target_filename = fs.GetFilename();

        if (target_directory != nullptr && target_filename != nullptr) {
            target_description.format("Target: {}/{}\nState: {}", fs.GetDirectory(),
                                      fs.GetFilename(), process_state);
        }
        else {
            target_description.format("Target: Unknown/Invalid (?)\nState: {}", process_state);
        }
        ImGui::TextUnformatted(target_description.data());
    }

    if (stopped) {
        if (ImGui::Button("Resume")) {
            get_process(app).Continue();
        }
    }
    else {
        if (ImGui::Button("Start")) {
            start_target(app);
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Stop")) {
        get_process(app).Stop();
    }

    ImGui::Separator();

    draw_file_browser(app, app.file_browser.get(), 0);
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginGroup();

    static float file_viewer_height = 0.6f * window_height_f;
    static float console_height = 0.4f * window_height_f;

    DEBUG_STREAM(console_height);
    DEBUG_STREAM(file_viewer_height);

    const float old_console_height = console_height;

    Splitter("##S2", false, 3.0f, &file_viewer_height, &console_height, 100, 100,
             file_viewer_width);

    if (window_resized) {
        file_viewer_height = file_viewer_height * (float)new_height / (float)old_height;
        console_height = console_height * (float)new_height / (float)old_height;
    }

    {  // start file viewer
        ImGui::BeginChild("FileViewer", ImVec2(file_viewer_width, file_viewer_height));
        if (ImGui::BeginTabBar("##FileViewerTabs", ImGuiTabBarFlags_AutoSelectNewTabs |
                                                       ImGuiTabBarFlags_NoTooltip)) {
            Defer(ImGui::EndTabBar());

            if (app.open_files.size() == 0) {
                if (ImGui::BeginTabItem("about")) {
                    Defer(ImGui::EndTabItem());
                    ImGui::TextUnformatted("This is a GUI for lldb.");
                }
            }
            else {
                draw_open_files(app);
            }
        }
        ImGui::EndChild();
    }  // end file viewer

    ImGui::Spacing();

    {  // start console/log
        ImGui::BeginChild("LogConsole",
                          ImVec2(file_viewer_width,
                                 console_height - 2 * ImGui::GetFrameHeightWithSpacing()));
        if (ImGui::BeginTabBar("##ConsoleLogTabs", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem("Console")) {
                ImGui::BeginChild("ConsoleEntries");

                // TODO: keep console input focused until user moves mouse

                for (const lldbg::CommandLineEntry& entry : app.command_line.get_history()) {
                    ImGui::TextColored(ImVec4(255, 0, 0, 255), "> %s", entry.input.c_str());
                    if (entry.succeeded) {
                        ImGui::TextUnformatted(entry.output.c_str());
                    }
                    else {
                        ImGui::Text("error: %s is not a valid command.", entry.input.c_str());
                    }

                    ImGui::TextUnformatted("\n");
                }

                // always scroll to the bottom of the command history after running a command
                const bool should_auto_scroll_command_window =
                    app.ui.ran_command_last_frame || old_console_height != console_height;

                auto command_input_callback = [](ImGuiTextEditCallbackData*) -> int {
                    return 0;
                };

                const ImGuiInputTextFlags command_input_flags =
                    ImGuiInputTextFlags_EnterReturnsTrue;

                static char input_buf[2048];
                if (ImGui::InputText("lldb console", input_buf, 2048, command_input_flags,
                                     command_input_callback)) {
                    run_lldb_command(app, input_buf);
                    memset(input_buf, 0, sizeof(input_buf));
                    input_buf[0] = '\0';
                    app.ui.ran_command_last_frame = true;
                }

                // Keep auto focus on the input box
                if (ImGui::IsItemHovered() ||
                    (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootWindow) &&
                     !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)))
                    ImGui::SetKeyboardFocusHere(-1);  // Auto focus previous widget

                if (should_auto_scroll_command_window) {
                    ImGui::SetScrollHere(1.0f);
                    app.ui.ran_command_last_frame = false;
                }

                ImGui::EndChild();

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Log")) {
                ImGui::BeginChild("LogEntries");
                lldbg::Logger::get_instance()->for_each_message(
                    [](const lldbg::LogMessage& message) -> void {
                        // TODO: colorize based on log level
                        ImGui::TextUnformatted(message.message.c_str());
                    });
                ImGui::SetScrollHere(1.0f);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("stdout")) {
                ImGui::BeginChild("StdOUTEntries");
                ImGui::TextUnformatted(app.stdout_buf.get());
                // ImGui::SetScrollHere(1.0f); // TODO: handle smart auto scrolling
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("stderr")) {
                ImGui::BeginChild("StdERREntries");
                ImGui::TextUnformatted(app.stderr_buf.get());
                // ImGui::SetScrollHere(1.0f); // TODO: handle smart auto scrolling
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
        ImGui::EndChild();
    }  // end console/log

    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginGroup();

    const float threads_height = (window_height - 2 * ImGui::GetFrameHeightWithSpacing()) / 4;
    const float stack_height = (window_height - 2 * ImGui::GetFrameHeightWithSpacing()) / 4;
    const float locals_height = (window_height - 2 * ImGui::GetFrameHeightWithSpacing()) / 4;
    const float breakpoint_height =
        (window_height - 2 * ImGui::GetFrameHeightWithSpacing()) / 4;

    ImGui::BeginChild(
        "#ThreadsChild",
        ImVec2(window_width - file_browser_width - file_viewer_width, threads_height));

    // TODO: be consistent about whether or not to use Defer
    if (ImGui::BeginTabBar("#ThreadsTabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Threads")) {
            if (stopped) {
                for (uint32_t i = 0; i < process.GetNumThreads(); i++) {
                    lldb::SBThread th = process.GetThreadAtIndex(i);

                    const char* thread_name = th.GetName();
                    if (!thread_name) {
                        LOG(Warning) << "Skipping thread with null name";
                        continue;
                    }

                    StringBuffer thread_label;
                    thread_label.format("Thread {}: {}", i, th.GetName());

                    if (ImGui::Selectable(thread_label.data(),
                                          (int)i == app.ui.viewed_thread_index)) {
                        app.ui.viewed_thread_index = i;
                    }
                }

                if (process.GetNumThreads() > 0 && app.ui.viewed_thread_index < 0) {
                    app.ui.viewed_thread_index = 0;
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::EndChild();

    ImGui::BeginChild("#StackTraceChild", ImVec2(0, stack_height));
    if (ImGui::BeginTabBar("##StackTraceTabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Stack Trace")) {
            static int selected_row = -1;

            if (stopped && app.ui.viewed_thread_index >= 0) {
                ImGui::Columns(3, "##StackTraceColumns");
                ImGui::Separator();
                ImGui::Text("FUNCTION");
                ImGui::NextColumn();
                ImGui::Text("FILE");
                ImGui::NextColumn();
                ImGui::Text("LINE");
                ImGui::NextColumn();
                ImGui::Separator();

                // TODO: use ImGuiSelectableFlags_SpanAllColumns as described here:
                // https://github.com/ocornut/imgui/issues/769
                lldb::SBThread viewed_thread =
                    process.GetThreadAtIndex(app.ui.viewed_thread_index);

                for (uint32_t i = 0; i < viewed_thread.GetNumFrames(); i++) {
                    auto frame = StackFrame::create(viewed_thread.GetFrameAtIndex(i));

                    if (!frame) continue;

                    if (ImGui::Selectable(frame->function_name.c_str(), (int)i == selected_row,
                                          ImGuiSelectableFlags_SpanAllColumns)) {
                        manually_open_and_or_focus_file(app, frame->file_handle);
                        selected_row = (int)i;
                    }
                    ImGui::NextColumn();

                    ImGui::TextUnformatted(frame->file_handle.filename().c_str());
                    ImGui::NextColumn();

                    StringBuffer linebuf;
                    linebuf.format("{}", (int)frame->line);
                    ImGui::TextUnformatted(linebuf.data());
                    ImGui::NextColumn();
                }

                app.ui.viewed_frame_index = selected_row;
                ImGui::Columns(1);
            }

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    ImGui::BeginChild("#LocalsChild", ImVec2(0, locals_height));
    if (ImGui::BeginTabBar("##LocalsTabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Locals")) {
            if (stopped && app.ui.viewed_frame_index >= 0) {
                ImGui::Columns(3, "##LocalsColumns");
                ImGui::Separator();
                ImGui::Text("NAME");
                ImGui::NextColumn();
                ImGui::Text("TYPE");
                ImGui::NextColumn();
                ImGui::Text("VALUE");
                ImGui::NextColumn();
                ImGui::Separator();

                lldb::SBThread viewed_thread =
                    process.GetThreadAtIndex(app.ui.viewed_thread_index);
                lldb::SBFrame frame = viewed_thread.GetFrameAtIndex(app.ui.viewed_frame_index);
                lldb::SBValueList locals = frame.GetVariables(true, true, true, true);

                for (uint32_t i = 0; i < locals.GetSize(); i++) {
                    lldb::SBValue local = locals.GetValueAtIndex(i);

                    const char* local_type = local.GetDisplayTypeName();
                    const char* local_name = local.GetName();
                    const char* local_value = local.GetValue();

                    if (!local_type || !local_name) {
                        continue;
                    }

                    // TODO: break this out into a recursive function to load children of
                    // children and so on
                    // TODO: double click on any entry to open a more detailed view in a pop
                    // out window
                    if (local.MightHaveChildren()) {
                        StringBuffer children_node_label;
                        children_node_label.format("{}##Children", local_name);

                        if (ImGui::TreeNode(children_node_label.data())) {
                            ImGui::NextColumn();
                            ImGui::TextUnformatted(local_type);
                            ImGui::NextColumn();
                            ImGui::TextUnformatted("...");
                            ImGui::NextColumn();
                            for (uint32_t i = 0; i < local.GetNumChildren(100); i++) {
                                lldb::SBValue child = local.GetChildAtIndex(i);
                                const char* child_type = child.GetTypeName();
                                const char* child_name = child.GetName();
                                const char* child_value = child.GetValue();

                                ImGui::TextUnformatted(child_name);
                                ImGui::NextColumn();
                                ImGui::TextUnformatted(child_type);
                                ImGui::NextColumn();
                                ImGui::TextUnformatted(child_value);
                                ImGui::NextColumn();
                            }
                            ImGui::TreePop();
                        }
                        else {
                            ImGui::NextColumn();
                            ImGui::TextUnformatted(local_type);
                            ImGui::NextColumn();
                            ImGui::TextUnformatted("...");
                            ImGui::NextColumn();
                        }
                    }
                    else {
                        ImGui::TextUnformatted(local_name);
                        ImGui::NextColumn();
                        ImGui::TextUnformatted(local_type);
                        ImGui::NextColumn();
                        ImGui::TextUnformatted(local_value);
                        ImGui::NextColumn();
                    }
                }

                ImGui::Columns(1);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Registers")) {
            ImGui::BeginChild("RegisterContents");
            // FIXME: why does this stall the program?
            // if (stopped && app.ui.viewed_frame_index >= 0) {
            //     lldb::SBThread viewed_thread =
            //     process.GetThreadAtIndex(app.ui.viewed_thread_index);
            //     lldb::SBFrame frame =
            //     viewed_thread.GetFrameAtIndex(app.ui.viewed_frame_index);
            //     lldb::SBValueList register_collections = frame.GetRegisters();

            //     for (uint32_t i = 0; i < register_collections.GetSize(); i++) {
            //         lldb::SBValue register_set = register_collections.GetValueAtIndex(i);
            //         //const std::string label = std::string(register_set.GetName()) +
            //         std::to_string(i); const std::string label = "Configuration##" +
            //         std::to_string(i);

            //         if (FileTreeNode(label.c_str())) {

            //             for (uint32_t i = 0; register_set.GetNumChildren(); i++) {
            //                 lldb::SBValue child = register_set.GetChildAtIndex(i);
            //                 if (child.GetName()) {
            //                     ImGui::TextUnformatted(child.GetName());
            //                 }
            //             }

            //             ImGui::TreePop();
            //         }
            //     }
            // }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    ImGui::BeginChild("#BreakWatchPointChild", ImVec2(0, breakpoint_height));
    if (ImGui::BeginTabBar("##BreakWatchPointTabs", ImGuiTabBarFlags_None)) {
        Defer(ImGui::EndTabBar());

        if (ImGui::BeginTabItem("Breakpoints")) {
            Defer(ImGui::EndTabItem());

            if (stopped && app.ui.viewed_thread_index >= 0) {
                static int selected_row = -1;

                ImGui::Columns(2);
                ImGui::Separator();
                ImGui::Text("FILE");
                ImGui::NextColumn();
                ImGui::Text("LINE");
                ImGui::NextColumn();
                ImGui::Separator();
                Defer(ImGui::Columns(1));

                lldb::SBTarget target = app.debugger.GetSelectedTarget();
                for (uint32_t i = 0; i < target.GetNumBreakpoints(); i++) {
                    lldb::SBBreakpoint breakpoint = target.GetBreakpointAtIndex(i);
                    lldb::SBBreakpointLocation location = breakpoint.GetLocationAtIndex(0);

                    if (!location.IsValid()) {
                        LOG(Error) << "Invalid breakpoint location encountered!";
                    }

                    lldb::SBAddress address = location.GetAddress();

                    if (!address.IsValid()) {
                        LOG(Error) << "Invalid lldb::SBAddress for breakpoint!";
                    }

                    lldb::SBLineEntry line_entry = address.GetLineEntry();

                    // TODO: save description and don't rebuild every frame
                    const std::string filename =
                        build_string(line_entry.GetFileSpec().GetFilename());
                    if (ImGui::Selectable(filename.c_str(), (int)i == selected_row)) {
                        // TODO: factor out
                        const std::string directory =
                            build_string(line_entry.GetFileSpec().GetDirectory()) + "/";
                        const std::string full_path = directory + filename;
                        manually_open_and_or_focus_file(app, full_path.c_str());
                        selected_row = (int)i;
                    }
                    ImGui::NextColumn();

                    StringBuffer line_buf;
                    line_buf.format("{}", line_entry.GetLine());
                    ImGui::Selectable(line_buf.data(), (int)i == selected_row);
                    ImGui::NextColumn();
                }
            }
        }

        if (ImGui::BeginTabItem("Watchpoints")) {
            Defer(ImGui::EndTabItem());

            for (int i = 0; i < 4; i++) {
                StringBuffer label;
                label.format("Watch {}", i);
                if (ImGui::Selectable(label.data(), i == 0)) {
                    // blah
                }
            }
        }
    }

    ImGui::EndChild();
    ImGui::EndGroup();
    ImGui::PopFont();
    ImGui::End();

    {
        ImGui::SetNextWindowPos(ImVec2(window_width / 2.f, window_height / 2.f),
                                ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Debug Stream", 0)) {
            for (const auto& [xkey, xstr] : s_debug_stream) {
                StringBuffer debug_line;
                debug_line.format("{} : {}", xkey, xstr);
                ImGui::TextUnformatted(debug_line.data());
            }
        }
        ImGui::End();
    }
}

static void tick(lldbg::Application& app)
{
    // process all queued LLDB events before drawing each frame
    while (true) {
        lldb::SBEvent event;
        const bool found_event = app.event_listener.pop_event(event);
        if (!found_event) break;

        const lldb::StateType new_state = lldb::SBProcess::GetStateFromEvent(event);
        const char* state_descr = lldb::SBDebugger::StateAsCString(new_state);
        LOG(Debug) << "Found event with new state: " << state_descr;

        // TODO: make this actually be useful
        if (new_state == lldb::eStateExited) {
            lldbg::ExitDialog dialog;
            dialog.process_name = "blah";
            dialog.exit_code = get_process(app).GetExitStatus();
            app.exit_dialog = dialog;
            LOG(Debug) << "Set exit dialog";
        }
    }

    lldbg::draw(app);

    std::optional<int> line_clicked = app.text_editor.line_clicked_this_frame;
    if (line_clicked) {
        add_breakpoint_to_viewed_file(app, *line_clicked);
    }
}

void Application::main_loop()
{
    static size_t frame_number = 0;

    while (!glfwWindowShouldClose(ui.window)) {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        tick(*this);

        ImGui::Render();
        glfwGetFramebufferSize(ui.window, &ui.window_width, &ui.window_height);
        glViewport(0, 0, ui.window_width, ui.window_height);
        static const ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(ui.window);

        // TODO: develop bettery strategy for when to read stdout,
        // possible upon receiving certian types of LLDBEvent?
        // TODO: define some sort of ProcessContext to keep the stdout per process/target
        if ((frame_number % 10 == 0) && has_target(*this)) {
            stdout_buf.update(get_process(*this));
            stderr_buf.update(get_process(*this));
        }

        frame_number++;
    }

    debugger.GetSelectedTarget().GetProcess().Kill();
}

// TODO: add return code and rename init_graphics
int initialize_rendering(UserInterface& ui)
{
    glfwSetErrorCallback(glfw_error_callback);

    if (glfwInit() != GLFW_TRUE) {
        return -1;
    }

    ui.window = glfwCreateWindow(1920, 1080, "lldbg", nullptr, nullptr);

    if (ui.window == nullptr) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(ui.window);
    glfwSwapInterval(1);  // Enable vsync

    const GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "GLEW Error: %s\n", glewGetErrorString(err));
        return -1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

    io.Fonts->AddFontDefault();
    static const std::string font_path =
        fmt::format("{}/ttf/Hack-Regular.ttf", LLDBG_ASSETS_DIR);
    ui.font = io.Fonts->AddFontFromFileTTF(font_path.c_str(), 15.0f);

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic();

    // disable all 'rounding' of corners in the UI
    ImGui::GetStyle().WindowRounding = 0.0f;
    ImGui::GetStyle().ChildRounding = 0.0f;
    ImGui::GetStyle().FrameRounding = 0.0f;
    ImGui::GetStyle().GrabRounding = 0.0f;
    ImGui::GetStyle().PopupRounding = 0.0f;
    ImGui::GetStyle().ScrollbarRounding = 0.0f;
    ImGui::GetStyle().TabRounding = 0.0f;

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(ui.window, true);
    ImGui_ImplOpenGL2_Init();

    return 0;
}

}  // namespace lldbg

namespace lldbg {

Application::Application()
    : stdout_buf(StreamBuffer::StreamSource::StdOut),
      stderr_buf(StreamBuffer::StreamSource::StdErr)
{
    lldb::SBDebugger::Initialize();
    debugger = lldb::SBDebugger::Create();
    debugger.SetAsync(true);
    command_line.replace_interpreter(debugger.GetCommandInterpreter());
    command_line.run_command("settings set auto-confirm 1", true);

    // TODO: let use choose disassembly flavor in drop down menu
    command_line.run_command("settings set target.x86-disassembly-flavor intel", true);

    initialize_rendering(this->ui);

    text_editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    TextEditor::Palette pal = text_editor.GetPalette();
    pal[(int)TextEditor::PaletteIndex::Breakpoint] =
        ImGui::GetColorU32(ImVec4(255, 0, 0, 255));
    text_editor.SetPalette(pal);
}

Application::~Application()
{
    event_listener.stop(debugger);
    lldb::SBDebugger::Terminate();

    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(this->ui.window);
    glfwTerminate();
}

void set_workdir(Application& app, const std::string& workdir)
{
    if (fs::exists(workdir) && fs::is_directory(workdir)) {
        app.file_browser = FileBrowserNode::create(workdir);
    }
    else {
        LOG(Error) << "Attempted to set working directory to non-existant path: " << workdir;
        app.file_browser = FileBrowserNode::create(fs::current_path());
    }
}

TargetAddResult add_target(Application& app, const std::string& exe_path)
{
    if (has_target(app)) {
        LOG(Error) << "Attempted add multiple targets, which is not (yet) supported by lldbg.";
        return TargetAddResult::TooManyTargetsError;
    }

    const fs::path full_exe_path = fs::canonical(exe_path);

    if (!fs::exists(full_exe_path)) {
        LOG(Error) << "Requested executable does not exist: {}" << full_exe_path;
        return TargetAddResult::ExeDoesNotExistError;
    }

    lldb::SBError lldb_error;
    lldb::SBTarget new_target =
        app.debugger.CreateTarget(full_exe_path.c_str(), nullptr, nullptr, true, lldb_error);

    if (!lldb_error.Success()) {
        const char* lldb_error_cstr = lldb_error.GetCString();
        LOG(Error) << (lldb_error_cstr ? lldb_error_cstr : "Unknown target creation error.");
        return TargetAddResult::TargetCreateError;
    }

    LOG(Debug) << "Succesfully added target for executable: " << full_exe_path;

    return TargetAddResult::Success;
}

TargetStartResult start_target(Application& app)
{
    if (!has_target(app)) {
        LOG(Warning) << "Failed to start target because no target is currently specified.";
        return TargetStartResult::NoTargetError;
    }

    app.stdout_buf.clear();
    app.stderr_buf.clear();

    lldb::SBTarget target = app.debugger.GetTargetAtIndex(0);
    lldb::SBLaunchInfo launch_info(const_cast<const char**>(app.argv));
    launch_info.SetLaunchFlags(lldb::eLaunchFlagDisableASLR | lldb::eLaunchFlagStopAtEntry);
    lldb::SBError lldb_error;
    lldb::SBProcess process = target.Launch(launch_info, lldb_error);

    if (!lldb_error.Success()) {
        const char* lldb_error_cstr = lldb_error.GetCString();
        LOG(Error) << (lldb_error_cstr ? std::string(lldb_error_cstr)
                                       : "Unknown target launch error!");
        LOG(Error) << "Failed to launch process, destroying target...";
        app.debugger.DeleteTarget(target);
        return TargetStartResult::LaunchError;
    }

    // TODO: print exe filepath here
    LOG(Debug) << "Succesfully launched process.";

    size_t ms_attaching = 0;
    while (process.GetState() == lldb::eStateAttaching) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ms_attaching += 100;
        if (ms_attaching / 1000 > 5) {
            LOG(Error) << "Attach timeout after launching target process.";
            return TargetStartResult::AttachTimeoutError;
        }
    }

    // TODO: print exe filepath here
    LOG(Debug) << "Succesfully attached to process.";

    app.event_listener.start(app.debugger);

    return TargetStartResult::Success;
}

}  // namespace lldbg
