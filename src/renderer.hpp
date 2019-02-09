#pragma once

#include <memory>
#include <vector>
#include <experimental/filesystem>
#include <unordered_map>

#include "lldb/API/LLDB.h"

namespace {

struct FileBrowserNode {
    std::vector<std::unique_ptr<FileBrowserNode>> children;
};

struct File {
    std::vector<std::string> lines;
};

struct FileCache {
    std::unordered_map<std::string, File> cache;
};

class FileBrowser {
    std::unique_ptr<FileBrowserNode> m_top;
public:
    void cd(const std::string& path);
};


}

namespace lldbg {

struct StackTraceEntry {
    std::string function_name;
    std::string file_name;
    std::string line;
    std::string column;

    StackTraceEntry(lldb::SBFrame frame)
    {
        const char* _function_name = frame.GetDisplayFunctionName();
        if (_function_name) {
            this->function_name = std::string(_function_name);
        } else {
            this->function_name = "unknown";
        }

        bool file_found;

        const char* _file_name = frame.GetLineEntry().GetFileSpec().GetFilename();
        if (_file_name) {
            this->file_name = std::string(_file_name);
            file_found = true;
        } else {
            this->file_name = "unknown";
            file_found = false;
        }

        if (file_found) {
            this->line = std::to_string(frame.GetLineEntry().GetLine());
            this->column = std::to_string(frame.GetLineEntry().GetColumn());
        } else {
            this->line = "unknown";
            this->column = "unknown";
        }
    }
};

inline void update_stack_trace(lldb::SBThread viewed_thread, std::vector<StackTraceEntry>& stack_trace)
{
    stack_trace.clear();

    for (uint32_t i = 0; i < viewed_thread.GetNumFrames(); i++) {
        StackTraceEntry new_entry(viewed_thread.GetFrameAtIndex(i));
        stack_trace.push_back(new_entry);
    }
}

struct RenderState {
    int viewed_thread_index = -1;
    int viewed_frame_index = -1;
    int window_width = -1;
    int window_height = -1;

    std::vector<StackTraceEntry> stack_trace;

    //FileBrowser file_browser;
};

void draw(lldb::SBProcess process, RenderState& state);

}
