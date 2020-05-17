#pragma once

#include <assert.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "Log.hpp"
#include "lldb/API/LLDB.h"

namespace fs = std::filesystem;

namespace lldbg {

enum class FileReadError { DoesNotExist, NotRegularFile };

class FileHandle {
    size_t m_hash;

    // TODO: record last file read/access time in separate static map
    // so that file contents can be dynamically reloaded when they change.
    static std::map<size_t, std::string> s_filepath_cache;
    static std::map<size_t, std::string> s_filename_cache;
    static std::map<size_t, std::vector<std::string>> s_contents_cache;

    FileHandle(size_t h) : m_hash(h) {}

public:
    FileHandle(void) = delete;

    inline friend bool operator==(const FileHandle& a, const FileHandle& b)
    {
        return a.m_hash == b.m_hash;
    }

    inline friend bool operator<(const FileHandle& a, const FileHandle& b)
    {
        return a.m_hash < b.m_hash;
    }

    static std::optional<FileHandle> create(const std::string& filepath);

    inline const std::vector<std::string>& contents(void) const
    {
        auto it = s_contents_cache.find(m_hash);
        assert(it != s_contents_cache.end());
        return it->second;
    }

    inline const std::string& filepath(void) const
    {
        auto it = s_filepath_cache.find(m_hash);
        assert(it != s_filepath_cache.end());
        return it->second;
    }

    inline const std::string& filename(void) const
    {
        auto it = s_filename_cache.find(m_hash);
        assert(it != s_filename_cache.end());
        return it->second;
    }
};

struct FileReference {
    const std::vector<std::string>* contents;
    std::filesystem::path canonical_path;
    std::string short_name;

    FileReference(std::vector<std::string>* _contents, std::filesystem::path _canonical_path)
        : contents(_contents),
          canonical_path(_canonical_path),
          short_name(canonical_path.filename().string())
    {
    }
};

class OpenFilesNew {
    std::vector<FileHandle> m_files;
    std::optional<size_t> m_focus;

    void close(size_t tab_index);

public:
    bool open(const std::string& filepath);

    inline size_t size() const { return m_files.size(); }

    inline std::optional<FileHandle> focus()
    {
        if (m_focus) {
            return m_files[*m_focus];
        }
        else {
            return {};
        }
    }

    enum class Action { Nothing, ChangeFocusTo, Close };

    template <typename Callable>
    void for_each_open_file(Callable&& f)
    {
        if (m_files.empty()) return;

        std::optional<size_t> tab_idx_to_close = {};
        std::optional<size_t> tab_idx_to_focus = {};

        assert(m_focus);
        const size_t focused_tab_index = *m_focus;

        for (size_t i = 0; i < m_files.size(); i++) {
            switch (f(m_files[i], i == focused_tab_index)) {
                case Action::ChangeFocusTo:
                    tab_idx_to_focus = i;
                    break;
                case Action::Close:
                    tab_idx_to_close = i;
                    break;
                case Action::Nothing:
                    break;
            }
        }

        if (tab_idx_to_close) this->close(*tab_idx_to_close);
        if (tab_idx_to_focus) m_focus = *tab_idx_to_focus;
    }
};

class OpenFiles {
    std::unordered_map<std::string, std::vector<std::string>> m_cache;

    std::vector<FileReference> m_refs;
    std::optional<size_t> m_focus;

    const std::variant<FileReadError, FileReference> read_from_disk(
        const std::filesystem::path& canonical_path);

public:
    bool open(const std::string& filepath);
    void close(const std::string& filepath);
    size_t size() const { return m_refs.size(); }

    std::optional<FileReference> focus()
    {
        if (m_focus) {
            return m_refs[*m_focus];
        }
        else {
            return {};
        }
    }

    enum class Action { Nothing, ChangeFocusTo, Close };

    template <typename Callable>
    void for_each_open_file(Callable&& f);
};

template <typename Callable>
void OpenFiles::for_each_open_file(Callable&& f)
{
    for (size_t i = 0; i < m_refs.size(); i++) {
        switch (f(m_refs[i], i == *m_focus)) {
            case Action::Nothing:
                break;
            case Action::ChangeFocusTo:
                m_focus = i;
                break;
            case Action::Close:
                m_refs.erase(m_refs.begin() + i);
                if (m_refs.empty()) {
                    m_focus = {};
                }
                else {
                    m_focus = i - 1;
                }
                break;
        }
    }
}

class BreakPointSetNew {
    std::map<FileHandle, std::unordered_set<int>> m_cache;

public:
    // void add(FileHandle handle, int line);
    // void remove(FileHandle handle, int line);
    void synchronize(lldb::SBTarget target);

    const std::unordered_set<int>* Get(FileHandle handle);

    // size_t NumBreakPoints(void)
    // {
    //     size_t s = 0;
    //     for (const auto& [_, bps] : m_cache) {
    //         s += bps.size();
    //     }
    //     return s;
    // }
};

class BreakPointSet {
    std::map<std::string, std::unordered_set<int>> m_cache;

public:
    void Synchronize(lldb::SBTarget target);
    void Add(const std::string& file, int line);
    void Remove(const std::string& file, int line);
    const std::unordered_set<int> Get(const std::string& file);

    // size_t NumBreakPoints(void)
    // {
    //     size_t s = 0;
    //     for (auto& it : m_cache) {
    //         s += it.second.size();
    //     }
    //     return s;
    // }
};

class FileBrowserNode {
    bool m_already_opened;
    const std::filesystem::path m_path;
    const std::string m_filename;
    const bool m_is_directory;

    FileBrowserNode(const std::filesystem::path& validated_path)
        : m_already_opened(false),
          m_path(validated_path),
          m_filename(validated_path.filename().string()),
          m_is_directory(std::filesystem::is_directory(validated_path))
    {
    }

public:
    // TODO: use std::string argument?
    static std::unique_ptr<FileBrowserNode> create(const std::filesystem::path& relative_path);
    static std::unique_ptr<FileBrowserNode> create(const char* relative_path);

    FileBrowserNode()
        : m_already_opened(false),
          m_path(std::filesystem::canonical(std::filesystem::current_path())),
          m_filename(m_path.has_filename() ? m_path.filename().string() : m_path.string()),
          m_is_directory(std::filesystem::is_directory(m_path))
    {
    }

    void open_children();

    std::vector<std::unique_ptr<FileBrowserNode>> children;
    const char* full_path() const { return m_path.c_str(); }
    const char* filename() const { return m_filename.c_str(); }
    bool is_directory() const { return m_is_directory; }
};

}  // namespace lldbg

