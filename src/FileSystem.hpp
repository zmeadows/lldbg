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

struct FileReference {
    std::vector<std::string>* contents;
    fs::path canonical_path;
    std::string short_name;

    FileReference(std::vector<std::string>* _contents, fs::path _canonical_path)
        : contents(_contents)
        , canonical_path(_canonical_path)
        , short_name(canonical_path.filename().string())
    {}
};


class FileStorage {
    std::unordered_map<std::string, std::vector<std::string>> m_cache;

public:
    std::unique_ptr<FileReference> read(const std::string& requested_filepath);
    std::unique_ptr<FileReference> read(const fs::path& canonical_path);
};

class OpenFiles {
    std::vector<FileReference> m_refs;
    int m_focus = -1;

public:
    void open(FileStorage& storage, const std::string& requested_filepath, bool take_focus = true);
    void close(const std::string& requested_filepath);
    void close(size_t idx);
    void change_focus(size_t idx);

    size_t size(void) const { return m_refs.size(); }
    int focus_index() const { return m_focus; }
    const FileReference& get_file_at_index(size_t i) const { return m_refs[i]; }
    const FileReference& focus() const { return m_refs[m_focus]; }
};

//TODO: rename FileBrowserNode for clarity
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

