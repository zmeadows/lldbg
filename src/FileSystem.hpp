#pragma once

#include "Log.hpp"

#include <assert.h>
#include <experimental/filesystem>
#include <fstream>
#include <memory>
#include <unordered_map>
#include <vector>

namespace fs = std::experimental::filesystem;

namespace lldbg {

// Keeping this as simple as possible for now.
// Each file is stored as a std::vector of std::string.
// A quick benchmark suggests loading a 3k line file takes 0.5ms.
// So we can safely load ~10 typical files per frame if needed.
// That should be enough for now (or ever?).
class FileStorage {
    std::unordered_map<std::string, std::vector<std::string>> m_cache;

public:
    const std::vector<std::string>* read(const std::string& filepath);
};

struct FileTreeNode {
    std::vector<std::unique_ptr<FileTreeNode>> children;
    const fs::path location; // canonical
    const bool is_directory;

    static std::unique_ptr<FileTreeNode> create(const fs::path& relative_path);
    static std::unique_ptr<FileTreeNode> create(const char* relative_location);

    void open_children();

private:
    FileTreeNode(const fs::path& new_path)
        : location(new_path), is_directory(fs::is_directory(new_path)) {}
};

}

