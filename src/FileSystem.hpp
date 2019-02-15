#pragma once

#include "Log.hpp"

#include <assert.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>

namespace lldbg {

enum class FileReadError {
    DoesNotExist,
    NotRegularFile
};

struct FileReference {
    std::vector<std::string>* contents;
    std::filesystem::path canonical_path;
    std::string short_name;

    FileReference(std::vector<std::string>* _contents, std::filesystem::path _canonical_path)
        : contents(_contents)
        , canonical_path(_canonical_path)
        , short_name(canonical_path.filename().string())
    {}
};

class OpenFiles {
    // IDEA: timestamp last time file was open, and release memory if it has been >X minutes?
    std::unordered_map<std::string, std::vector<std::string>> m_cache;

    std::vector<FileReference> m_refs;
    std::optional<size_t> m_focus;


    using FileReadResult = std::variant<FileReadError, FileReference>;

    FileReadResult read_from_disk(const std::filesystem::path& canonical_path);

public:
    bool open(const std::string& filepath);
    void close(const std::string& filepath);
    size_t size() const { return m_refs.size(); }

    enum class Action { ChangeFocusTo, Close };

    template <typename Callable>
    void for_each_open_file(Callable&& f);
};

template <typename Callable>
void OpenFiles::for_each_open_file(Callable&& f) {
    for (auto i = 0; i < m_refs.size(); i++) {
        std::optional<Action> maybe_action = f(m_refs[i], i == *m_focus);

        if (!maybe_action) {
            continue;
        }

        switch (*maybe_action) {
            case Action::ChangeFocusTo:
                m_focus = i;
                break;
            case Action::Close:
                m_refs.erase(m_refs.begin() + i);
                if (m_refs.empty()) {
                    m_focus = {};
                } else {
                    m_focus = i - 1;
                }
                break;
        }
    }
}

//TODO: rename FileBrowserNode for clarity
struct FileTreeNode {
    std::vector<std::unique_ptr<FileTreeNode>> children;
    const std::filesystem::path location; // canonical
    const bool is_directory;

    static std::unique_ptr<FileTreeNode> create(const std::filesystem::path& relative_path);
    static std::unique_ptr<FileTreeNode> create(const char* relative_location);

    void open_children();

private:
    FileTreeNode(const std::filesystem::path& new_path)
        : location(new_path), is_directory(std::filesystem::is_directory(new_path)) {}
};

}

