#pragma once

#include "Log.hpp"
#include "Prelude.hpp"

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



class OpenFiles {
    std::unordered_map<std::string, std::vector<std::string>> m_cache;

    std::vector<FileReference> m_refs;
    optional<size_t> m_focus;

    optional<FileReference> read(const std::string& requested_filepath);
    optional<FileReference> read(const fs::path& canonical_path);

public:
    using OpenFileIndex = size_t;

    const optional<FileReference> open(const std::string& requested_filepath);
    void close(OpenFileIndex idx);

    void set_focus(OpenFileIndex idx);
    bool is_focused(OpenFileIndex idx) const { return m_focus ? idx == *m_focus : false; }

    size_t size(void) const { return m_refs.size(); }

    const optional<FileReference> operator[](OpenFileIndex i) const {
        if (i < m_refs.size()) {
            return m_refs[i];
        } else {
            return {};
        }
    }

    const optional<FileReference> focus() const {
        if (m_focus) {
            return m_refs[*m_focus];
        } else {
            return {};
        }
    }
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

