#pragma once

#include <lldb/API/LLDB.h>

#include <cassert>
#include <iostream>

#include "FPSTimer.hpp"
#include "FileSystem.hpp"
#include "FileViewer.hpp"
#include "LLDBCommandLine.hpp"
#include "LLDBEventListenerThread.hpp"
#include "Log.hpp"
#include "StreamBuffer.hpp"

// clang-format off
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl2.h"
// clang-format on

#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace lldbg {

// TODO: create sane Constructor and initialize UserInterface and Application *before* adding
// targets in main
// TODO: use std::optional<size_t> instead of -1
// TODO: add size_t to keep track of frames_rendered
struct UserInterface {
    int viewed_thread_index = -1;
    int viewed_frame_index = -1;

    float window_width = -1.f;   // in pixels
    float window_height = -1.f;  // in pixels

    float file_browser_width = -1.f;
    float file_viewer_width = -1.f;
    float file_viewer_height = -1.f;
    float console_height = -1.f;

    bool request_manual_tab_change = false;
    bool ran_command_last_frame = false;
    bool window_resized_last_frame = false;

    ImFont* font = nullptr;
    GLFWwindow* window = nullptr;
};

// TODO: convert this struct to OOP-style
struct Application {
    std::unique_ptr<DebugSession> session;
    OpenFiles open_files;
    BreakPointSet breakpoints;  // TODO: move to TextEditor
    std::unique_ptr<lldbg::FileBrowserNode> file_browser;
    UserInterface ui;
    FileViewer text_editor;
    FPSTimer fps_timer;

    int main_loop(void);

    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application& operator=(Application&&) = delete;
};

void set_workdir(Application& app, const std::string& workdir);

}  // namespace lldbg
