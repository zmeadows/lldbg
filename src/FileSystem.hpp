#pragma once

#include <assert.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
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
    // This will involve inotify on linux and kqueue on macos
    static std::map<size_t, std::string> s_filepath_cache;
    static std::map<size_t, std::string> s_filename_cache;
    static std::map<size_t, std::vector<std::string>> s_contents_cache;
    static std::mutex s_mutex;

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

    const std::vector<std::string>& contents(void);
    const std::string& filepath(void);
    const std::string& filename(void);
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
                    assert(!tab_idx_to_focus);
                    tab_idx_to_focus = i;
                    break;
                case Action::Close:
                    assert(!tab_idx_to_close);
                    tab_idx_to_close = i;
                    break;
                case Action::Nothing:
                    break;
            }
        }

        if (tab_idx_to_close) this->close(*tab_idx_to_close);
        if (tab_idx_to_focus) m_focus = tab_idx_to_focus;
    }
};

class BreakPointSet {
    std::map<FileHandle, std::unordered_set<int>> m_cache;

public:
    // void add(FileHandle handle, int line);
    // void remove(FileHandle handle, int line);
    void synchronize(lldb::SBTarget target);

    const std::unordered_set<int>* Get(FileHandle handle);
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

