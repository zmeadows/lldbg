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

    const std::variant<FileReadError, FileReference> read_from_disk(const std::filesystem::path& canonical_path);

public:
    bool open(const std::string& filepath);
    void close(const std::string& filepath);
    size_t size() const { return m_refs.size(); }

    const std::optional<FileReference> focus() {
        if (m_focus) {
            return m_refs[*m_focus];
        } else {
            return {};
        }
    }

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

// class BreakPointSet {
//     std::unordered_map<std::string, std::set<size_t>> m_cache;
// 
// public:
//     void Synchronize(lldb::SBTarget target);
// 
//     void Add(const std::string& file, size_t line);
//     void Remove(const std::string& file, size_t line);
//     void Clear(void);
// };

class FileBrowserNode {
    bool m_already_opened;
    const std::filesystem::path m_path;
    const std::string m_filename;
    const bool m_is_directory;

    FileBrowserNode(const std::filesystem::path& validated_path)
        : m_already_opened(false)
        , m_path(validated_path)
        , m_filename(validated_path.filename().string())
        , m_is_directory(std::filesystem::is_directory(validated_path))
    {}

public:
    static std::unique_ptr<FileBrowserNode> create(const std::filesystem::path& relative_path);
    static std::unique_ptr<FileBrowserNode> create(const char* relative_location);

    FileBrowserNode()
        : m_already_opened(false)
        , m_path(std::filesystem::canonical(std::filesystem::current_path()))
        , m_filename(m_path.has_filename() ? m_path.filename().string() : m_path.string())
        , m_is_directory(std::filesystem::is_directory(m_path))
    {}

    void open_children();

    std::vector<std::unique_ptr<FileBrowserNode>> children;
    const char* full_path() const { return m_path.c_str(); }
    const char* filename() const { return m_filename.c_str(); }
    bool is_directory() const { return m_is_directory; }
};

}

