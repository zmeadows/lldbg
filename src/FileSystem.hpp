#pragma once

#include "Log.hpp"

#include <assert.h>
#include <experimental/filesystem>
#include <fstream>
#include <memory>
#include <unordered_map>
#include <vector>

namespace fs = std::experimental::filesystem;

namespace {

std::vector<std::string> read_lines(const fs::path& filepath) {
    assert(fs::is_regular_file(filepath));

    std::ifstream infile(filepath.c_str());

    std::vector<std::string> contents;

    std::string line;
    while (std::getline(infile, line)) {
        contents.push_back(line);
    }

    return contents;
}

class FileStorage {
    std::unordered_map<std::string, std::vector<std::string>> m_cache;

public:
    void cache(const std::string& filepath) {
        auto it = m_cache.find(filepath);
        if (it != m_cache.end()) {
            LOG(Error) << "Attempted to cache file that has already been cached: " << filepath;
            return;
        }
        m_cache.emplace(filepath, read_lines(filepath));
    }

    const std::vector<std::string>* get(const std::string& filepath) const {
        const auto it = m_cache.find(filepath);
        if (it == m_cache.end()) {
            LOG(Error) << "Attempted to access contents of file that hasn't been cached: " << filepath;
            return nullptr;
        }
        return &it->second;
    }
};

struct FileTreeNode {
    std::vector<std::unique_ptr<FileTreeNode>> children;
    const fs::path location; // canonical
    const bool is_directory;

    static std::unique_ptr<FileTreeNode> create(const fs::path& relative_path) {
        if (!fs::exists(relative_path)) return nullptr;
        fs::path canonical_path = fs::canonical(relative_path);

        if (!fs::is_directory(canonical_path) && !fs::is_regular_file(canonical_path)) {
            LOG(Error) << "Attemped to load a path ("
                       << canonical_path
                       << ") that wasn't a directory or regular file!";
            return nullptr;
        }

        return std::unique_ptr<FileTreeNode>(new FileTreeNode(canonical_path));
    }

    static std::unique_ptr<FileTreeNode> create(const char* relative_location) {
        return FileTreeNode::create(fs::path(relative_location));
    }

    void open_children() {
        if (this->children.empty()) {
            for (const auto& p : fs::directory_iterator(this->location)) {
                this->children.push_back(FileTreeNode::create(p));
            }
        }
    }


private:
    FileTreeNode(const fs::path& new_path)
        : location(new_path), is_directory(fs::is_directory(new_path)) {}
};

}

namespace lldbg {

class FileBrowser {
    FileStorage m_store;
    std::unique_ptr<FileTreeNode> m_base_node;

public:
    FileBrowser() {}
    FileBrowser(const char* location) : m_base_node(FileTreeNode::create(location)) {}

    bool replace_base_node(const char* location) {
        auto new_node = FileTreeNode::create(location);
        std::swap(m_base_node, new_node);
        return static_cast<bool>(m_base_node);
    }

    FileTreeNode* base_node(void) { return m_base_node.get(); }
};

}

