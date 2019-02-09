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

// A convenience struct for extracting pertinent display information from an lldb::SBFrame
struct StackFrameDescription {
    const char* function_name = nullptr;
    const char* file_name = nullptr;
    int line = -1;
    int column = -1;

    static StackFrameDescription build(lldb::SBFrame frame) {
        StackFrameDescription description;

        description.function_name = frame.GetDisplayFunctionName();
        description.file_name = frame.GetLineEntry().GetFileSpec().GetFilename();
        description.line = (int) frame.GetLineEntry().GetLine();
        description.column = (int) frame.GetLineEntry().GetColumn();

        return description;
    }
};

struct RenderState {
    int viewed_thread_index = -1;
    int viewed_frame_index = -1;
    int window_width = -1;
    int window_height = -1;

    //FileBrowser file_browser;
};

void draw(lldb::SBProcess process, RenderState& state);

}
