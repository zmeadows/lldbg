#pragma once

#include <lldb/API/LLDB.h>

#include <cassert>
#include <iostream>

#include "DebugSession.hpp"
#include "EventListener.hpp"
#include "FPSTimer.hpp"
#include "FileSystem.hpp"
#include "FileViewer.hpp"
#include "LLDBCommandLine.hpp"
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

// TODO: turn into class with OOP style
struct Application {
    DebugSession session;
    OpenFiles open_files;
    BreakPointSet
        breakpoints;  // TODO: move to TextEditor, as this is just for visual marking purposes
    std::unique_ptr<FileBrowserNode>
        file_browser;  // TODO convert to non-pointer with default constructor in cwd
    UserInterface ui;  // TODO use UserInterface constructor to initializer graphics
    FileViewer text_editor;
    FPSTimer fps_timer;

    int main_loop(void);
    void set_workdir(const std::string& workdir);

    // TODO: take UserInterface&& as argument
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application& operator=(Application&&) = delete;
};
