#include "FileSystem.hpp"

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

}

namespace lldbg {

std::unique_ptr<FileTreeNode> FileTreeNode::create(const fs::path& relative_path) {
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

std::unique_ptr<FileTreeNode> FileTreeNode::create(const char* relative_location) {
    return FileTreeNode::create(fs::path(relative_location));
}

void FileTreeNode::open_children() {
    if (this->children.empty()) {
        for (const auto& p : fs::directory_iterator(this->location)) {
            this->children.push_back(FileTreeNode::create(p));
        }
    }
}

const std::vector<std::string>* FileStorage::read(const std::string& filepath) {
    const auto it = m_cache.find(filepath);
    if (it == m_cache.end()) {
        auto it = m_cache.emplace(filepath, read_lines(filepath));
        const std::vector<std::string>& contents = it.first->second;
        return &contents;
    } else {
        return &it->second;
    }
}

}
