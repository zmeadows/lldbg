#include "FileSystem.hpp"

#include "Log.hpp"

#include <algorithm>

namespace {

std::vector<std::string> read_lines(const fs::path& filepath) {
    assert(fs::is_regular_file(filepath));

    std::ifstream infile(filepath.c_str());

    std::vector<std::string> contents;

    std::string line;
    while (std::getline(infile, line)) {
        contents.push_back(line);
    }

    LOG(Debug) << "Read file from disk: " << filepath;

    return contents;
}

}

namespace lldbg {

std::unique_ptr<FileTreeNode> FileTreeNode::create(const fs::path& relative_path) {
    if (!fs::exists(relative_path)) { return nullptr; }
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

        std::sort(this->children.begin(), this->children.end(),
                  [](const std::unique_ptr<FileTreeNode>& a, const std::unique_ptr<FileTreeNode>& b) {
                      if (a->is_directory && !b->is_directory) {
                          return true;
                      } else if (!a->is_directory && b->is_directory) {
                          return false;
                      } else {
                          return a->location.string() < b->location.string();
                      }
                  });

    }
}

optional<FileReference> OpenFiles::read(const std::string& requested_filepath) {
    return read(fs::canonical(requested_filepath));
}

optional<FileReference> OpenFiles::read(const fs::path& canonical_path) {
    const std::string canonical_path_str = canonical_path.string();

    const auto it = m_cache.find(canonical_path_str);

    if (it == m_cache.end()) { // file hasn't been cached already
        if (!fs::exists(canonical_path)) {
            LOG(Error) << "Attempted to add non-existent file to cache: " << canonical_path;
            return {};
        }

        if (!fs::is_regular_file(canonical_path)) {
            LOG(Error) << "Attempted to add something other than regular file to cache: " << canonical_path;
            return {};
        }

        auto it = m_cache.emplace(canonical_path_str, read_lines(canonical_path));
        return FileReference(&it.first->second, canonical_path);

    } else { // found the file in the cache
        return FileReference(&it->second, canonical_path);
    }
}

const optional<FileReference> OpenFiles::open(const std::string& requested_filepath) {
    const fs::path canonical_path = fs::canonical(requested_filepath);

    auto it = std::find_if(m_refs.begin(), m_refs.end(), [&](const FileReference& ref) {
        return ref.canonical_path == canonical_path;
    });

    if (it != m_refs.end()) { //already in set
        LOG(Info) << "Attempted to add file already in OpenFiles: " << canonical_path;

        m_focus = it - m_refs.begin();

        return {};
    }

    optional<FileReference> ref_ptr = read(canonical_path);

    if (!ref_ptr) { return {}; }

    m_refs.emplace_back(*ref_ptr);

    LOG(Debug) << "Opened file " << requested_filepath << " and now have " << size() << " files open.";

    m_focus = m_refs.size() - 1;

    return m_refs.back();
}

void OpenFiles::set_focus(size_t idx) {
    if (idx < size()) {
        m_focus = idx;
    } else {
        LOG(Error) << "Attempted to change focus to an invalid index of "
                   << idx << " with an OpenFiles set of size " << size();
    }
}

// void OpenFiles::close(const std::string& requested_filepath) {
//     const fs::path canonical_path = fs::canonical(requested_filepath);
//
//     auto it = std::find_if(m_refs.begin(), m_refs.end(), [&](const FileReference& ref) {
//         return ref.canonical_path == canonical_path;
//     });
//
//     if (it == m_refs.end()) {
//         LOG(Error) << "Attempted to remove unopen file from open files list: " << canonical_path;
//         return;
//     }
//
//     const size_t remove_idx = it - m_refs.begin();
//     m_refs.erase(it);
//     m_focus = static_cast<int>(remove_idx) - 1;
// }

void OpenFiles::close(size_t idx) {
    assert(idx < size());
    m_refs.erase(m_refs.begin() + idx);

    if (m_refs.empty()) {
        m_focus = {};
    } else {
        assert(idx > 0);
        m_focus = idx - 1;
    }
}

}
