#include "FileSystem.hpp"

#include "Log.hpp"
#include "Prelude.hpp"

#include <algorithm>

namespace {

std::vector<std::string> read_lines(const std::filesystem::path& filepath) {
    assert(std::filesystem::is_regular_file(filepath));

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

std::unique_ptr<FileTreeNode> FileTreeNode::create(const std::filesystem::path& relative_path) {
    if (!std::filesystem::exists(relative_path)) { return nullptr; }
    std::filesystem::path canonical_path = std::filesystem::canonical(relative_path);

    if (!std::filesystem::is_directory(canonical_path) && !std::filesystem::is_regular_file(canonical_path)) {
        LOG(Error) << "Attemped to load a path ("
                   << canonical_path
                   << ") that wasn't a directory or regular file!";
        return nullptr;
    }

    return std::unique_ptr<FileTreeNode>(new FileTreeNode(canonical_path));
}

std::unique_ptr<FileTreeNode> FileTreeNode::create(const char* relative_location) {
    return FileTreeNode::create(std::filesystem::path(relative_location));
}

void FileTreeNode::open_children() {
    if (this->children.empty()) {
        for (const auto& p : std::filesystem::directory_iterator(this->location)) {
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

std::variant<FileReadError, FileReference>
OpenFiles::read_from_disk(const std::filesystem::path& canonical_path)
{
    const std::string canonical_path_str = canonical_path.string();

    const auto old_it = m_cache.find(canonical_path_str);

    if (old_it == m_cache.end()) { // file hasn't been cached already
        if (!std::filesystem::exists(canonical_path)) {
            return FileReadError::DoesNotExist;
        }

        if (!std::filesystem::is_regular_file(canonical_path)) {
            return FileReadError::NotRegularFile;
        }

        auto new_it = m_cache.emplace(canonical_path_str, read_lines(canonical_path));

        std::vector<std::string>* new_contents = &new_it.first->second;

        return FileReference(new_contents, canonical_path);

    } else { // found the file in the cache
        return FileReference(&old_it->second, canonical_path);
    }
}

bool OpenFiles::open(const std::string& requested_filepath) {
    const std::filesystem::path canonical_path = std::filesystem::canonical(requested_filepath);

    auto it = std::find_if(m_refs.begin(), m_refs.end(), [&](const FileReference& ref) {
        return ref.canonical_path == canonical_path;
    });

    if (it != m_refs.end()) { //already in set
        m_focus = it - m_refs.begin();
        return true;
    }

    return MATCH( read_from_disk(canonical_path),

        [&](const FileReadError& error) {
            switch (error) {
                case FileReadError::DoesNotExist:
                    LOG(Warning) << "Attempted to open non-existent file: " << requested_filepath;
                    break;
                case FileReadError::NotRegularFile:
                    LOG(Warning) << "Attempted to open something other than a regular file (maybe a directory?): " << requested_filepath;
                    break;
            };

            return false;
        },

        [&](const FileReference& ref) {
            LOG(Verbose) << "Successfully opened file: " << requested_filepath;
            m_refs.emplace_back(ref);
            m_focus = m_refs.size() - 1;

            LOG(Debug) << "Number of open files: " << size();
            return true;
        }
    );

}

void OpenFiles::close(const std::string& filepath) {
    const std::filesystem::path path_to_close = std::filesystem::canonical(filepath);
    for_each_open_file([path_to_close](const FileReference& ref, bool) -> std::optional<Action> {
        if (ref.canonical_path == path_to_close) {
            return Action::Close;
        } else {
            return {};
        }
    });
}

}
