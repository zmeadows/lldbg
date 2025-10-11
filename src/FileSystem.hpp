#pragma once

#include <assert.h>

#include <algorithm>
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

class FileHandle {
  size_t m_hash;

  // TODO: possibly record last file read/access time in separate static map
  // so that file contents can be dynamically reloaded when they change.
  // This will involve inotify on linux and kqueue on macos.

  static std::map<size_t, std::string> s_filepath_cache;
  static std::map<size_t, std::string> s_filename_cache;
  static std::map<size_t, std::vector<std::string>> s_contents_cache;
  static std::mutex s_mutex; // all static std::map access must be thread-safe

  FileHandle(size_t h) : m_hash(h) {}

public:
  FileHandle(void) = delete;

  static std::optional<FileHandle> create(const std::string &filepath);

  const std::vector<std::string> &contents(void);
  const std::string &filepath(void);
  const std::string &filename(void);

  inline friend bool operator==(const FileHandle &a, const FileHandle &b) {
    return a.m_hash == b.m_hash;
  }

  inline friend bool operator<(const FileHandle &a, const FileHandle &b) {
    return a.m_hash < b.m_hash;
  }
};

class OpenFiles {
  std::vector<FileHandle> m_files;
  std::optional<size_t> m_focus;
  std::vector<std::optional<size_t>> m_files_linum;

  void close(size_t tab_index);

public:
  bool open(const std::string &filepath);
  void open(FileHandle handle);
  void open(FileHandle handle, size_t linum);

  inline size_t size() const { return m_files.size(); }

  inline std::optional<FileHandle> focus() {
    if (m_focus) {
      return m_files[*m_focus];
    } else {
      return {};
    }
  }

  inline std::pair<std::optional<FileHandle>, std::optional<size_t>>
  focus_line() {
    auto file = focus();
    if (file) {
      return {file, m_files_linum[*m_focus]};
    } else {
      return {};
    }
  }

  inline void update_focus_line(FileHandle handle, size_t linum) {
    auto it = std::find(m_files.begin(), m_files.end(), handle);
    if (it != m_files.end()) {
      m_files_linum[it - m_files.begin()] = std::make_optional<size_t>(linum);
    } else {
      LOG(Error) << "There is no file in opened files! Try Open thme first";
    }
  }

  enum class Action { Nothing, ChangeFocusTo, Close };

  // Beyond simply looping over the open files, the supplied Callable can
  // optionally return an 'Action' to be applied to each specific file, as
  // defined above.
  template <typename Callable> void for_each_open_file(Callable &&f) {
    if (m_files.empty())
      return;

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

    if (tab_idx_to_close)
      this->close(*tab_idx_to_close);
    if (tab_idx_to_focus)
      m_focus = tab_idx_to_focus;
  }
};

class FileBrowserNode {
  fs::path m_filepath;
  fs::path m_filename;
  bool m_opened;

  void open_children();
  std::vector<std::unique_ptr<FileBrowserNode>> m_children;

  FileBrowserNode() = delete;

  FileBrowserNode(fs::path validated_path)
      : m_filepath(fs::canonical(validated_path)),
        m_filename(m_filepath.filename()), m_opened(false) {}

public:
  static std::unique_ptr<FileBrowserNode>
  create(std::optional<fs::path> path_request);

  const char *filepath() const { return m_filepath.c_str(); }
  const char *filename() const { return m_filename.c_str(); }
  bool is_directory() const { return fs::is_directory(m_filepath); }

  inline const std::vector<std::unique_ptr<FileBrowserNode>> &children(void) {
    this->open_children();
    return m_children;
  }
};
