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
#include "ImGuiFileDialog.h"

namespace fs = std::filesystem;

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

static std::pair<fs::path, int> resolve_breakpoint(lldb::SBBreakpointLocation location)
{
    lldb::SBAddress address = location.GetAddress();
    lldb::SBLineEntry line_entry = address.GetLineEntry();
    const char* filename = line_entry.GetFileSpec().GetFilename();
    const char* directory = line_entry.GetFileSpec().GetDirectory();

    if (filename == nullptr || directory == nullptr) {
        LOG(Error) << "Failed to read breakpoint location after thread halted";
        return {fs::path(), -1};
    }
    else {
        return {fs::path(directory) / fs::path(filename), line_entry.GetLine()};
    }
}

static std::pair<bool, bool> process_is_finished(lldb::SBProcess& process)
{
    if (!process.IsValid()) {
        return {false, false};
    }

    const lldb::StateType state = process.GetState();
    const bool exited = state == lldb::eStateExited;
    const bool failed = state == lldb::eStateCrashed;

    return {exited || failed, !failed};
}

static bool process_is_running(lldb::SBProcess& process)
{
    return process.IsValid() && process.GetState() == lldb::eStateRunning;
}

static bool process_is_stopped(lldb::SBProcess& process)
{
    const auto state = process.GetState();
    return process.IsValid() && (state == lldb::eStateStopped || state == lldb::eStateUnloaded);
}

static void stop_process(lldb::SBProcess& process)
{
    if (!process.IsValid()) {
        LOG(Warning) << "Attempted to stop an invalid process.";
        return;
    }

    if (process_is_stopped(process)) {
        LOG(Warning) << "Attempted to stop an already-stopped process.";
        return;
    }

    lldb::SBError err = process.Stop();
    if (err.Fail()) {
        LOG(Error) << "Failed to stop the process, encountered the following error: "
                   << err.GetCString();
        return;
    }
}

/*
static void continue_process(lldb::SBProcess& process)
{
    if (!process.IsValid()) {
        LOG(Warning) << "Attempted to continue an invalid process.";
        return;
    }

    if (process_is_running(process)) {
        LOG(Warning) << "Attempted to continue an already-running process.";
        return;
    }

    lldb::SBError err = process.Continue();
    if (err.Fail()) {
        LOG(Error) << "Failed to continue the process, encountered the following error: "
                   << err.GetCString();
        return;
    }
}
*/

static void kill_process(lldb::SBProcess& process)
{
    if (!process.IsValid()) {
        LOG(Warning) << "Attempted to kill an invalid process.";
        return;
    }

    if (process_is_finished(process).first) {
        LOG(Warning) << "Attempted to kill an already-finished process.";
        return;
    }

    lldb::SBError err = process.Kill();
    if (err.Fail()) {
        LOG(Error) << "Failed to kill the process, encountered the following error: \n\t"
                   << err.GetCString();
        return;
    }
}

static std::string build_string(const char* cstr)
{
    return cstr ? std::string(cstr) : std::string();
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
          function_name(std::move(_function_name)),
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

        if (!fs::exists(directory)) {
            // LOG(Warning) << "Directory specified by lldb stack frame doesn't exist: " <<
            // directory; LOG(Warning) << "Filepath specified: " << filename;
            return {};
        }

        if (auto handle = FileHandle::create(directory / filename); handle.has_value()) {
            return StackFrame(*handle, (int)frame.GetLineEntry().GetLine(),
                              (int)frame.GetLineEntry().GetColumn(),
                              build_string(frame.GetDisplayFunctionName()));
        }
        else {
            LOG(Warning) << "Filepath corresponding to lldb stack frame doesn't exist: "
                         << directory / filename;
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
                     float* size2, float min_size1, float min_size2, float splitter_long_axis_size)
{
    using namespace ImGui;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiID id = window->GetID(name);
    ImRect bb;
    bb.Min =
        window->DC.CursorPos + (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
    bb.Max = bb.Min + CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size)
                                                    : ImVec2(splitter_long_axis_size, thickness),
                                   0.0f, 0.0f);
    return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2,
                            min_size1, min_size2, 0.0f);
}

static void draw_open_files(Application& app)
{
    bool closed_tab = false;

    app.open_files.for_each_open_file([&](FileHandle handle, bool is_focused) {
        auto action = OpenFiles::Action::Nothing;

        // we programmatically set the focused tab if manual tab change requested
        // for example when the user clicks an entry in the stack trace or file explorer
        auto tab_flags = ImGuiTabItemFlags_None;
        if (app.ui.request_manual_tab_change && is_focused) {
            tab_flags = ImGuiTabItemFlags_SetSelected;
            app.file_viewer.show(handle);
        }

        bool keep_tab_open = true;
        if (ImGui::BeginTabItem(handle.filename().c_str(), &keep_tab_open, tab_flags)) {
            ImGui::BeginChild("FileContents");
            if (!app.ui.request_manual_tab_change && !is_focused) {
                // user selected tab directly with mouse
                action = OpenFiles::Action::ChangeFocusTo;
                app.file_viewer.show(handle);
            }
            std::optional<int> clicked_line = app.file_viewer.render();
            if (clicked_line.has_value()) {
                std::optional<FileHandle> focus_handle = app.open_files.focus();
                if (focus_handle.has_value()) {
                    const fs::path filepath = focus_handle->filepath();
                    StringBuffer breakpoint_command;
                    breakpoint_command.format("breakpoint set --file {} --line {}",
                                              filepath.c_str(), *clicked_line);
                    run_lldb_command(app, breakpoint_command.data());
                }
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (!keep_tab_open) {  // user closed tab with mouse
            closed_tab = true;
            action = OpenFiles::Action::Close;
        }

        return action;
    });

    app.ui.request_manual_tab_change = false;

    if (closed_tab && app.open_files.size() > 0) {
        auto focus_handle = app.open_files.focus();
        if (!focus_handle.has_value()) {
            LOG(Error) << "Invalid logic encountered when user requested tab close.";
        }
        else {
            app.file_viewer.show(*focus_handle);
        }
    }
}

static void manually_open_and_or_focus_file(UserInterface& ui, OpenFiles& open_files,
                                            FileHandle handle)
{
    if (auto focus = open_files.focus(); focus.has_value() && (*focus == handle)) {
        return;  // already focused
    }

    open_files.open(handle);
    ui.request_manual_tab_change = true;
}

static void manually_open_and_or_focus_file(UserInterface& ui, OpenFiles& open_files,
                                            const char* filepath)
{
    if (auto handle = FileHandle::create(filepath); handle.has_value()) {
        manually_open_and_or_focus_file(ui, open_files, *handle);
    }
    else {
        LOG(Warning) << "Failed to switch focus to file because it could not be located: "
                     << filepath;
    }
}

static void draw_file_browser(Application& app, FileBrowserNode* node_to_draw, size_t depth)
{
    assert(node_to_draw);

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
            manually_open_and_or_focus_file(app.ui, app.open_files, node_to_draw->filepath());
        }
    }
}

static std::optional<lldb::SBTarget> find_target(lldb::SBDebugger& debugger)
{
    if (debugger.GetNumTargets() > 0) {
        auto target = debugger.GetSelectedTarget();
        if (!target.IsValid()) {
            LOG(Warning) << "Selected target is invalid";
            return {};
        }
        else {
            return target;
        }
    }
    else {
        return {};
    }
}

static std::optional<lldb::SBProcess> find_process(lldb::SBDebugger& debugger)
{
    auto target = find_target(debugger);
    if (!target.has_value()) return {};

    lldb::SBProcess process = target->GetProcess();

    if (process.IsValid()) {
        return process;
    }
    else {
        return {};
    }
}

static lldb::SBCommandReturnObject run_lldb_command(lldb::SBDebugger& debugger,
                                                    LLDBCommandLine& cmdline,
                                                    const lldb::SBListener& listener,
                                                    const char* command,
                                                    bool hide_from_history = false)
{
    if (auto unaliased_cmd = cmdline.expand_and_unalias_command(command);
        unaliased_cmd.has_value()) {
        LOG(Debug) << "Unaliased command: " << *unaliased_cmd;
    }

    auto target_before = find_target(debugger);
    lldb::SBCommandReturnObject ret = cmdline.run_command(command, hide_from_history);
    auto target_after = find_target(debugger);

    const bool added_new_target = !target_before && target_after;
    const bool switched_target = target_before && target_after && (*target_before != *target_after);

    if (added_new_target || switched_target) {
        constexpr auto target_listen_flags = lldb::SBTarget::eBroadcastBitBreakpointChanged |
                                             lldb::SBTarget::eBroadcastBitWatchpointChanged;
        target_after->GetBroadcaster().AddListener(listener, target_listen_flags);
    }

    switch (ret.GetStatus()) {
        case lldb::eReturnStatusInvalid:
            LOG(Debug) << "\t => eReturnStatusInvalid";
            break;
        case lldb::eReturnStatusSuccessFinishNoResult:
            LOG(Debug) << "\t => eReturnStatusSuccessFinishNoResult";
            break;
        case lldb::eReturnStatusSuccessFinishResult:
            LOG(Debug) << "\t => eReturnStatusSuccessFinishResult";
            break;
        case lldb::eReturnStatusSuccessContinuingNoResult:
            LOG(Debug) << "\t => eReturnStatusSuccessContinuingNoResult";
            break;
        case lldb::eReturnStatusSuccessContinuingResult:
            LOG(Debug) << "\t => eReturnStatusSuccessContinuingResult";
            break;
        case lldb::eReturnStatusStarted:
            LOG(Debug) << "\t => eReturnStatusStarted";
            break;
        case lldb::eReturnStatusFailed:
            LOG(Debug) << "\t => eReturnStatusFailed";
            break;
        case lldb::eReturnStatusQuit:
            LOG(Debug) << "\t => eReturnStatusQuit";
            break;
        default:
            LOG(Debug) << "unknown lldb command return status encountered.";
            break;
    }

    return ret;
}

lldb::SBCommandReturnObject run_lldb_command(Application& app, const char* command,
                                             bool hide_from_history)
{
    return run_lldb_command(app.debugger, app.cmdline, app.listener, command, hide_from_history);
}

static void draw_control_bar(lldb::SBDebugger& debugger, LLDBCommandLine& cmdline,
                             const lldb::SBListener& listener, const UserInterface& ui)
{
    auto target = find_target(debugger);
    if (target.has_value()) {
        // TODO: show rightmost chunk of path in case it is too long to fit on screen
        lldb::SBFileSpec fs = target->GetExecutable();
        StringBuffer target_description;
        const char* target_directory = fs.GetDirectory();
        const char* target_filename = fs.GetFilename();

        if (target_directory != nullptr && target_filename != nullptr) {
            target_description.format("Target: {}/{}", target_directory, target_filename);
        }
        else {
            target_description.format("Target: Unknown/Invalid (?)");
        }
        ImGui::TextUnformatted(target_description.data());
    }

    auto process = find_process(debugger);
    if (process.has_value()) {
        StringBuffer process_description;
        const char* process_state = lldb::SBDebugger::StateAsCString(process->GetState());
        process_description.format("Process State: {}", process_state);
        ImGui::TextUnformatted(process_description.data());
    }
    else if (target.has_value()) {
        ImGui::TextUnformatted("Process State: Unlaunched");
    }

    if (!target.has_value()) {
        if (ImGui::Button("choose target")) {
            // TODO: use https://github.com/aiekick/ImGuiFileDialog
            IGFD::FileDialogConfig config;
            config.path = ".";
            ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", "((.*))", config);
        }

        std::string filePathName;

        if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
                LOG(Info) << "Selected File Path Name: " << filePathName;

                //TODO: Check if the file is an executable
                run_lldb_command(debugger, cmdline, listener, std::string("target create " + filePathName).c_str());

            }

            ImGuiFileDialog::Instance()->Close();
        }

    }
    else if (!process.has_value()) {
        if (ImGui::Button("run")) {
            run_lldb_command(debugger, cmdline, listener, "run");
        }
    }
    else if (process_is_stopped(*process)) {
        if (ImGui::Button("continue")) {
            run_lldb_command(debugger, cmdline, listener, "continue");
        }
        ImGui::SameLine();
        if (ImGui::Button("step over")) {
            const uint32_t nthreads = process->GetNumThreads();
            if (ui.viewed_thread_index < nthreads) {
                lldb::SBThread th = process->GetThreadAtIndex(ui.viewed_thread_index);
                th.StepOver();
            }
        }
        if (ImGui::Button("step into")) {
            const uint32_t nthreads = process->GetNumThreads();
            if (ui.viewed_thread_index < nthreads) {
                lldb::SBThread th = process->GetThreadAtIndex(ui.viewed_thread_index);
                th.StepInto();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("step instr.")) {
            LOG(Warning) << "Step over unimplemented";
        }

        if (ImGui::Button("restart")) {
            stop_process(*process);
            run_lldb_command(debugger, cmdline, listener, "run");
        }
    }
    else if (process_is_running(*process)) {
        if (ImGui::Button("stop")) {
            stop_process(*process);
        }
    }
    else if (const auto [finished, _] = process_is_finished(*process); finished) {
        if (ImGui::Button("restart")) {
            run_lldb_command(debugger, cmdline, listener, "run");
        }
    }
    else {
        LOG(Error) << "Unknown/Invalid session state encountered!";
    }
}

static void draw_file_viewer(Application& app)
{
    ImGui::BeginChild("FileViewer", ImVec2(app.ui.file_viewer_width, app.ui.file_viewer_height));
    if (ImGui::BeginTabBar("##FileViewerTabs",
                           ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_NoTooltip)) {
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
}

static void draw_console(Application& app)
{
    ImGui::BeginChild("LogConsole",
                      ImVec2(app.ui.file_viewer_width,
                             app.ui.console_height - 2 * ImGui::GetFrameHeightWithSpacing()));
    if (ImGui::BeginTabBar("##ConsoleLogTabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("console")) {
            ImGui::BeginChild("ConsoleEntries");

            for (const CommandLineEntry& entry : app.cmdline.get_history()) {
                ImGui::TextColored(ImVec4(255, 0, 0, 255), "> %s", entry.input.c_str());
                if (!entry.succeeded) {
                    ImGui::Text("error: %s is not a valid command.", entry.input.c_str());
                    continue;
                }

                if (entry.output.size() > 0) {
                    ImGui::TextUnformatted(entry.output.c_str());
                }
            }

            // later in this method we scroll to the bottom of the command history if
            // a command was run last frame, so that the user can immediately see the output.
            const bool should_auto_scroll_command_window =
                app.ui.ran_command_last_frame || app.ui.window_resized_last_frame;

            auto command_input_callback = [](ImGuiInputTextCallbackData*) -> int {
                return 0;  // TODO: scroll command line history with up/down arrows
            };

            const ImGuiInputTextFlags command_input_flags = ImGuiInputTextFlags_EnterReturnsTrue;

            // keep console input focused unless user is doing something else
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)) {
                ImGui::SetKeyboardFocusHere(0);
            }

            // TODO: resize input_buf when necessary?
            static char input_buf[2048];
            if (ImGui::InputText("lldb console", input_buf, 2048, command_input_flags,
                                 command_input_callback)) {
                run_lldb_command(app, input_buf);
                memset(input_buf, 0, sizeof(input_buf));
                input_buf[0] = '\0';
                app.ui.ran_command_last_frame = true;
            }

            // Always keep keyboard input focused on the lldb console input box unless
            // some other disrupting action is occuring
            if (ImGui::IsItemHovered() ||
                (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootWindow) &&
                 !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0))) {
                ImGui::SetKeyboardFocusHere(-1);  // Auto focus previous widget
            }

            if (should_auto_scroll_command_window) {
                ImGui::SetScrollHereY(1.0f);
                app.ui.ran_command_last_frame = false;
            }

            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("log")) {
            ImGui::BeginChild("LogEntries");
            Logger::get_instance()->for_each_message([](const LogMessage& entry) -> void {
                const char* msg = entry.message.c_str();
                switch (entry.level) {
                    case LogLevel::Verbose: {
                        ImGui::TextColored(ImVec4(78.f / 255.f, 78.f / 255.f, 78.f / 255.f, 255.f),
                                           "[VERBOSE]");
                        break;
                    }
                    case LogLevel::Debug: {
                        ImGui::TextColored(
                            ImVec4(52.f / 255.f, 56.f / 255.f, 176.f / 255.f, 255.f / 255.f),
                            "[DEBUG]");
                        break;
                    }
                    case LogLevel::Info: {
                        ImGui::TextColored(
                            ImVec4(225.f / 255.f, 225.f / 255.f, 225.f / 255.f, 255.f / 255.f),
                            "[INFO]");
                        break;
                    }
                    case LogLevel::Warning: {
                        ImGui::TextColored(
                            ImVec4(216.f / 255.f, 129.f / 255.f, 42.f / 255.f, 255.f / 255.f),
                            "[WARNING]");
                        break;
                    }
                    case LogLevel::Error: {
                        ImGui::TextColored(
                            ImVec4(212.f / 255.f, 67.f / 255.f, 67.f / 255.f, 255.f / 255.f),
                            "[ERROR]");
                        break;
                    }
                }
                ImGui::SameLine();
                ImGui::TextWrapped("%s", msg);
            });

            static size_t last_seen_messages = 0;

            const size_t seen_messages = Logger::get_instance()->message_count();
            if (seen_messages > last_seen_messages) {
                last_seen_messages = seen_messages;
                ImGui::SetScrollHereY(1.0f);
            }

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        // TODO: these quantities need to be reset whenever the target is reset or process is
        // re-launched
        static size_t last_stdout_size = 0;
        static size_t last_stderr_size = 0;

        if (ImGui::BeginTabItem("stdout")) {
            ImGui::BeginChild("StdOUTEntries");

            ImGui::TextUnformatted(app._stdout.get());
            if (app._stdout.size() > last_stdout_size) {
                ImGui::SetScrollHereY(1.0f);
            }
            last_stdout_size = app._stdout.size();

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("stderr")) {
            ImGui::BeginChild("StdERREntries");
            ImGui::TextUnformatted(app._stderr.get());
            if (app._stderr.size() > last_stderr_size) {
                ImGui::SetScrollHereY(1.0f);
            }
            last_stderr_size = app._stderr.size();
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::EndChild();
}

static void draw_threads(UserInterface& ui, std::optional<lldb::SBProcess> process,
                         float stack_height)
{
    ImGui::BeginChild(
        "#ThreadsChild",
        ImVec2(ui.window_width - ui.file_browser_width - ui.file_viewer_width, stack_height));

    Defer(ImGui::EndChild());

    // TODO: be consistent about whether or not to use Defer
    // TODO: add columns with stop reason and other potential information
    if (ImGui::BeginTabBar("#ThreadsTabs", ImGuiTabBarFlags_None)) {
        Defer(ImGui::EndTabBar());
        if (ImGui::BeginTabItem("threads")) {
            Defer(ImGui::EndTabItem());
            if (process.has_value() && process_is_stopped(*process)) {
                const uint32_t nthreads = process->GetNumThreads();

                if (ui.viewed_thread_index >= nthreads) {
                    ui.viewed_thread_index = nthreads - 1;
                    LOG(Warning) << "detected/fixed overflow of ui.viewed_thread_index";
                }

                StringBuffer thread_label;
                for (uint32_t i = 0; i < nthreads; i++) {
                    lldb::SBThread th = process->GetThreadAtIndex(i);

                    if (!th.IsValid()) {
                        LOG(Warning) << "Encountered invalid thread";
                        continue;
                    }

                    const char* thread_name = th.GetName();
                    if (thread_name == nullptr) {
                        LOG(Warning) << "Skipping thread with null name";
                        continue;
                    }

                    thread_label.format("Thread {}: {}", i, th.GetName());

                    if (ImGui::Selectable(thread_label.data(), i == ui.viewed_thread_index)) {
                        ui.viewed_thread_index = i;
                    }

                    thread_label.clear();
                }
            }
        }
    }
}

static void draw_stack_trace(UserInterface& ui, OpenFiles& open_files,
                             std::optional<lldb::SBProcess> process, float stack_height)
{
    ImGui::BeginChild("#StackTraceChild", ImVec2(0, stack_height));

    if (ImGui::BeginTabBar("##StackTraceTabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("stack trace")) {
            if (process.has_value() && process_is_stopped(*process)) {
                ImGui::Columns(3, "##StackTraceColumns");
                ImGui::Separator();
                ImGui::Text("FUNCTION");
                ImGui::NextColumn();
                ImGui::Text("FILE");
                ImGui::NextColumn();
                ImGui::Text("LINE");
                ImGui::NextColumn();
                ImGui::Separator();

                lldb::SBThread viewed_thread = process->GetThreadAtIndex(ui.viewed_thread_index);
                const uint32_t nframes = viewed_thread.GetNumFrames();

                if (ui.viewed_frame_index >= nframes) {
                    ui.viewed_frame_index = nframes - 1;
                }

                for (uint32_t i = 0; i < viewed_thread.GetNumFrames(); i++) {
                    auto frame = StackFrame::create(viewed_thread.GetFrameAtIndex(i));

                    if (!frame) continue;

                    if (ImGui::Selectable(frame->function_name.c_str(), i == ui.viewed_frame_index,
                                          ImGuiSelectableFlags_SpanAllColumns)) {
                        manually_open_and_or_focus_file(ui, open_files, frame->file_handle);
                        ui.viewed_frame_index = i;
                    }
                    ImGui::NextColumn();

                    ImGui::TextUnformatted(frame->file_handle.filename().c_str());
                    ImGui::NextColumn();

                    StringBuffer linebuf;
                    linebuf.format("{}", (int)frame->line);
                    ImGui::TextUnformatted(linebuf.data());
                    ImGui::NextColumn();
                }
                ImGui::Columns(1);
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::EndChild();
}

// TODO: add max depth?
static void draw_local_recursive(lldb::SBValue local)
{
    const char* local_type = local.GetDisplayTypeName();
    const char* local_name = local.GetName();
    const char* local_value = local.GetValue();

    if (!local_type || !local_name) {
        return;
    }

    StringBuffer children_node_label;
    children_node_label.format("{}##Children_{}", local_name, local.GetID());

    if (local.MightHaveChildren()) {
        if (ImGui::TreeNode(children_node_label.data())) {
            ImGui::NextColumn();
            ImGui::TextUnformatted(local_type);
            ImGui::NextColumn();
            ImGui::TextUnformatted("...");
            ImGui::NextColumn();

            // TODO: figure out best way to handle very long children list
            for (uint32_t i = 0; i < local.GetNumChildren(100); i++) {
                draw_local_recursive(local.GetChildAtIndex(i));
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
        if (local_value) {
            ImGui::TextUnformatted(local_value);
        }
        else {
            ImGui::TextUnformatted("unknown");
        }
        ImGui::NextColumn();
    }
}

static void draw_locals_and_registers(UserInterface& ui, std::optional<lldb::SBProcess> process,
                                      float stack_height)
{
    ImGui::BeginChild("#LocalsChild", ImVec2(0, stack_height));

    if (ImGui::BeginTabBar("##LocalsTabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("locals")) {
            if (process.has_value() && process_is_stopped(*process)) {
                ImGui::Columns(3, "##LocalsColumns");
                ImGui::Separator();
                ImGui::Text("NAME");
                ImGui::NextColumn();
                ImGui::Text("TYPE");
                ImGui::NextColumn();
                ImGui::Text("VALUE");
                ImGui::NextColumn();
                ImGui::Separator();

                lldb::SBThread viewed_thread = process->GetThreadAtIndex(ui.viewed_thread_index);
                lldb::SBFrame frame = viewed_thread.GetFrameAtIndex(ui.viewed_frame_index);
                lldb::SBValueList locals = frame.GetVariables(true, true, true, true);

                // TODO: select entire row like in stack trace
                for (uint32_t i = 0; i < locals.GetSize(); i++) {
                    draw_local_recursive(locals.GetValueAtIndex(i));
                }

                ImGui::Columns(1);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("registers")) {
            if (process.has_value() && process_is_stopped(*process)) {
                lldb::SBThread viewed_thread = process->GetThreadAtIndex(ui.viewed_thread_index);
                lldb::SBFrame frame = viewed_thread.GetFrameAtIndex(ui.viewed_frame_index);
                if (viewed_thread.IsValid() && frame.IsValid()) {
                    lldb::SBValueList register_collections = frame.GetRegisters();

                    for (uint32_t i = 0; i < register_collections.GetSize(); i++) {
                        lldb::SBValue regcol = register_collections.GetValueAtIndex(i);

                        const char* collection_name = regcol.GetName();

                        if (!collection_name) {
                            LOG(Warning) << "Skipping over invalid/un-named register collection";
                            continue;
                        }

                        StringBuffer reg_coll_name;
                        reg_coll_name.format("{}##RegisterCollection", collection_name);
                        if (ImGui::TreeNode(reg_coll_name.data())) {
                            for (uint32_t i = 0; i < regcol.GetNumChildren(); i++) {
                                lldb::SBValue reg = regcol.GetChildAtIndex(i);
                                const char* reg_name = reg.GetName();
                                const char* reg_value = reg.GetValue();

                                if (!reg_name || !reg_value) {
                                    LOG(Warning) << "skipping invalid register";
                                    continue;
                                }

                                ImGui::Text("%s = %s", reg_name, reg_value);
                            }

                            ImGui::TreePop();
                        }
                    }
                }
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
        // ImGui::SameLine(ImGui::GetWindowWidth() - 150);
        // ImGui::Checkbox("use hexadecimal", &use_hex_locals);
    }
    ImGui::EndChild();
}

static void draw_breakpoints_and_watchpoints(UserInterface& ui, OpenFiles& open_files,
                                             std::optional<lldb::SBTarget> target,
                                             float stack_height)
{
    ImGui::BeginChild("#BreakWatchPointChild", ImVec2(0, stack_height));
    if (ImGui::BeginTabBar("##BreakWatchPointTabs", ImGuiTabBarFlags_None)) {
        Defer(ImGui::EndTabBar());

        if (ImGui::BeginTabItem("breakpoints")) {
            Defer(ImGui::EndTabItem());

            // TODO: show hit count and column number as well
            if (target.has_value()) {
                ImGui::Columns(2);
                ImGui::Separator();
                ImGui::Text("FILE");
                ImGui::NextColumn();
                ImGui::Text("LINE");
                ImGui::NextColumn();
                ImGui::Separator();
                Defer(ImGui::Columns(1));

                const uint32_t nbreakpoints = target->GetNumBreakpoints();
                if (ui.viewed_breakpoint_index >= nbreakpoints) {
                    ui.viewed_breakpoint_index = nbreakpoints - 1;
                }

                for (uint32_t i = 0; i < nbreakpoints; i++) {
                    lldb::SBBreakpoint breakpoint = target->GetBreakpointAtIndex(i);

                    if (!breakpoint.IsValid() || breakpoint.GetNumLocations() == 0) {
                        lldb::SBStream stm;
                        breakpoint.GetDescription(stm);
                        LOG(Error) << "Invalid breakpoint encountered with description:\n"
                                   << stm.GetData();
                        continue;
                    }

                    lldb::SBBreakpointLocation location = breakpoint.GetLocationAtIndex(0);

                    if (!location.IsValid()) {
                        LOG(Error) << "Invalid breakpoint location encountered with "
                                   << breakpoint.GetNumLocations() << " locations!";
                        continue;
                    }

                    lldb::SBAddress address = location.GetAddress();

                    if (!address.IsValid()) {
                        LOG(Error) << "Invalid breakpoint address encountered!";
                        continue;
                    }

                    lldb::SBLineEntry line_entry = address.GetLineEntry();

                    if (!line_entry.IsValid()) {
                        LOG(Error) << "Invalid line entry encountered!";
                        continue;
                    }

                    const char* filename = line_entry.GetFileSpec().GetFilename();
                    const char* directory = line_entry.GetFileSpec().GetDirectory();
                    if (filename == nullptr || directory == nullptr) {
                        LOG(Error) << "invalid/unspecified filepath encountered for breakpoint";
                        continue;
                    }

                    if (ImGui::Selectable(filename, i == ui.viewed_breakpoint_index,
                                          ImGuiSelectableFlags_SpanAllColumns)) {
                        fs::path breakpoint_filepath = fs::path(directory) / fs::path(filename);
                        manually_open_and_or_focus_file(ui, open_files,
                                                        breakpoint_filepath.c_str());
                        ui.viewed_breakpoint_index = i;
                    }
                    ImGui::NextColumn();

                    StringBuffer line_buf;
                    line_buf.format("{}", line_entry.GetLine());
                    ImGui::TextUnformatted(line_buf.data());
                    ImGui::NextColumn();
                }
            }
        }

        if (ImGui::BeginTabItem("watchpoints")) {
            Defer(ImGui::EndTabItem());

            // TODO: add actual watch points
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
}

static void draw_debug_stream_popup(UserInterface& ui)
{
    ImGui::SetNextWindowPos(ImVec2(ui.window_width / 2.f, ui.window_height / 2.f),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);

    ImGui::PushFont(ui.font);

    if (ImGui::Begin("Debug Stream", 0)) {
        for (const auto& [xkey, xstr] : s_debug_stream) {
            StringBuffer debug_line;
            debug_line.format("{} : {}", xkey, xstr);
            ImGui::TextUnformatted(debug_line.data());
        }
    }

    ImGui::PopFont();

    ImGui::End();
}

__attribute__((flatten)) static void draw(Application& app)
{
    auto& ui = app.ui;
    auto& open_files = app.open_files;

    ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(ui.window_width, ui.window_height), ImGuiCond_Always);

    static constexpr auto main_window_flags =
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoTitleBar;

    ImGui::Begin("lldbg", 0, main_window_flags);
    ImGui::PushFont(ui.font);

    {
        Splitter("##S1", true, 3.0f, &ui.file_browser_width, &ui.file_viewer_width,
                 0.05 * ui.window_width, 0.05 * ui.window_width, ui.window_height);

        ImGui::BeginGroup();
        ImGui::BeginChild("ControlBarAndFileBrowser", ImVec2(ui.file_browser_width, 0));
        draw_control_bar(app.debugger, app.cmdline, app.listener, app.ui);
        ImGui::Separator();
        draw_file_browser(app, app.file_browser.get(), 0);
        ImGui::EndChild();
        ImGui::EndGroup();
    }

    ImGui::SameLine();

    {
        Splitter("##S2", false, 3.0f, &ui.file_viewer_height, &ui.console_height,
                 0.1 * ui.window_height, 0.1 * ui.window_height, ui.file_viewer_width);

        ImGui::BeginGroup();
        draw_file_viewer(app);
        ImGui::Spacing();
        draw_console(app);
        ImGui::EndGroup();
    }

    ImGui::SameLine();

    {
        ImGui::BeginGroup();

        // TODO: let locals tab have all the expanded space
        const float stack_height = (ui.window_height - 2 * ImGui::GetFrameHeightWithSpacing()) / 4;

        draw_threads(ui, find_process(app.debugger), stack_height);
        draw_stack_trace(ui, open_files, find_process(app.debugger), stack_height);
        draw_locals_and_registers(ui, find_process(app.debugger), stack_height);
        draw_breakpoints_and_watchpoints(ui, open_files, find_target(app.debugger), stack_height);

        ImGui::EndGroup();
    }

    ImGui::PopFont();
    ImGui::End();

    draw_debug_stream_popup(ui);
}

static void handle_lldb_events(lldb::SBDebugger& debugger, lldb::SBListener& listener,
                               UserInterface& ui, OpenFiles& open_files, FileViewer& file_viewer)
{
    lldb::SBEvent event;

    while (true) {
        const bool event_found = listener.GetNextEvent(event);
        if (!event_found) break;

        if (!event.IsValid()) {
            LOG(Warning) << "Invalid event found.";
            continue;
        }

        lldb::SBStream event_description;
        event.GetDescription(event_description);
        LOG(Verbose) << "Event Description => " << event_description.GetData();

        auto target = find_target(debugger);
        auto process = find_process(debugger);

        if (target.has_value() && event.BroadcasterMatchesRef(target->GetBroadcaster())) {
            LOG(Debug) << "Found target event";
            file_viewer.synchronize_breakpoint_cache(*target);
        }
        else if (process.has_value() && event.BroadcasterMatchesRef(process->GetBroadcaster())) {
            const lldb::StateType new_state = lldb::SBProcess::GetStateFromEvent(event);
            const char* state_descr = lldb::SBDebugger::StateAsCString(new_state);

            if (state_descr) {
                LOG(Debug) << "Found process event with new state: " << state_descr;
            }

            // For now we find the first (if any) stopped thread and construct a StopInfo.
            if (new_state == lldb::eStateStopped) {
                const uint32_t nthreads = process->GetNumThreads();
                for (uint32_t i = 0; i < nthreads; i++) {
                    lldb::SBThread th = process->GetThreadAtIndex(i);
                    switch (th.GetStopReason()) {
                        case lldb::eStopReasonBreakpoint: {
                            // https://lldb.llvm.org/cpp_reference/classlldb_1_1SBThread.html#af284261156e100f8d63704162f19ba76
                            assert(th.GetStopReasonDataCount() == 2);
                            lldb::break_id_t breakpoint_id = th.GetStopReasonDataAtIndex(0);
                            lldb::SBBreakpoint breakpoint =
                                target->FindBreakpointByID(breakpoint_id);

                            lldb::break_id_t location_id = th.GetStopReasonDataAtIndex(1);
                            lldb::SBBreakpointLocation location =
                                breakpoint.FindLocationByID(location_id);

                            const auto [filepath, linum] = resolve_breakpoint(location);
                            manually_open_and_or_focus_file(ui, open_files, filepath.c_str());
                            file_viewer.set_highlight_line(linum);
                            break;
                        }
                        default: {
                            continue;
                        }
                    }
                }
            }
            else if (new_state == lldb::eStateRunning) {
                file_viewer.unset_highlight_line();
            }
        }
        else {
            // TODO: print event description
            LOG(Debug) << "Found non-target/process event";
        }
    }
}

static void tick(Application& app)
{
    handle_lldb_events(app.debugger, app.listener, app.ui, app.open_files, app.file_viewer);

    UserInterface& ui = app.ui;
    DEBUG_STREAM(ui.window_width);
    DEBUG_STREAM(ui.window_height);
    DEBUG_STREAM(ui.file_browser_width);
    DEBUG_STREAM(ui.file_viewer_width);
    DEBUG_STREAM(ui.file_viewer_height);
    DEBUG_STREAM(ui.console_height);
    DEBUG_STREAM(app.fps_timer.current_fps());

    draw(app);
}

static void update_window_dimensions(UserInterface& ui)
{
    int new_width = -1;
    int new_height = -1;

    glfwGetFramebufferSize(ui.window, &new_width, &new_height);
    assert(new_width > 0 && new_height > 0);

    ui.window_resized_last_frame = new_width != ui.window_width || new_height != ui.window_height;

    if (ui.window_resized_last_frame) {
        // re-scale the size of the invididual panels to account for window resize
        ui.file_browser_width *= new_width / ui.window_width;
        ui.file_viewer_width *= new_width / ui.window_width;
        ui.file_viewer_height *= new_height / ui.window_height;
        ui.console_height *= new_height / ui.window_height;

        ui.window_width = new_width;
        ui.window_height = new_height;
    }
}

int main_loop(Application& app)
{
    while (!glfwWindowShouldClose(app.ui.window)) {
        glfwPollEvents();

        // TODO: switch to OpenGL 3 for performance
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        tick(app);

        ImGui::Render();
        glViewport(0, 0, app.ui.window_width, app.ui.window_height);
        static const ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

        glFinish();

        glfwSwapBuffers(app.ui.window);

        // TODO: develop bettery strategy for when to read stdout,
        // possible upon receiving certain types of LLDBEvent?
        if (app.ui.frames_rendered % 10 == 0) {
            if (auto process = find_process(app.debugger); process.has_value()) {
                app._stdout.update(*process);
                app._stderr.update(*process);
            }
        }

        update_window_dimensions(app.ui);

        app.fps_timer.wait_for_frame_duration(1.75 * 16666);
        app.fps_timer.frame_end();
        app.ui.frames_rendered++;
    }

    return EXIT_SUCCESS;
}

std::optional<UserInterface> UserInterface::init(void)
{
    UserInterface ui;

    glfwSetErrorCallback(glfw_error_callback);

    if (glfwInit() != GLFW_TRUE) {
        return {};
    }

    // TODO: use function to choose initial window resolution based on display resolution
    ui.window = glfwCreateWindow(1920, 1080, "lldbg", nullptr, nullptr);

    if (ui.window == nullptr) {
        glfwTerminate();
        return {};
    }

    ui.window_width = 1920.f;
    ui.window_height = 1080.f;
    ui.file_browser_width = ui.window_width * 0.15f;
    ui.file_viewer_width = ui.window_width * 0.52f;
    ui.file_viewer_height = ui.window_height * 0.6f;
    ui.console_height = ui.window_height * 0.4f;

    glfwMakeContextCurrent(ui.window);
    glfwSwapInterval(0);  // disable vsync

    const GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "GLEW Error: %s\n", glewGetErrorString(err));
        return {};
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

    io.Fonts->AddFontDefault();
    static const std::string font_path = fmt::format("{}/ttf/Hack-Regular.ttf", LLDBG_ASSETS_DIR);
    ui.font = io.Fonts->AddFontFromFileTTF(font_path.c_str(), 15.0f);

    ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic();

    // disable all 'rounding' of corners in the UI
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.TabRounding = 0.0f;

    ImGui_ImplGlfw_InitForOpenGL(ui.window, true);
    ImGui_ImplOpenGL2_Init();

    return ui;
}

Application::Application(const UserInterface& ui_, std::optional<fs::path> workdir)
    : debugger(lldb::SBDebugger::Create()),
      listener(debugger.GetListener()),
      cmdline(debugger),
      _stdout(StreamBuffer::StreamSource::StdOut),
      _stderr(StreamBuffer::StreamSource::StdErr),
      file_browser(FileBrowserNode::create(workdir)),
      ui(ui_)
{
}

Application::~Application()
{
    if (auto process = find_process(this->debugger); process.has_value() && process->IsValid()) {
        LOG(Warning) << "Found active process while closing Application.";
        kill_process(*process);
    }

    if (auto target = find_target(this->debugger); target.has_value() && target->IsValid()) {
        LOG(Warning) << "Found active target while closing Application.";
        this->debugger.DeleteTarget(*target);
    }

    if (this->debugger.IsValid()) {
        lldb::SBDebugger::Destroy(this->debugger);
        this->debugger.Clear();
    }
    else {
        LOG(Warning) << "Found invalid lldb::SBDebugger while closing Application.";
    }

    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(this->ui.window);
    glfwTerminate();
}

