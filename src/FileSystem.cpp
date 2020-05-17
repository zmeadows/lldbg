#include "FileSystem.hpp"

#include <algorithm>
#include <cstring>

#include "Log.hpp"
#include "Prelude.hpp"
#include "fmt/format.h"

static std::vector<std::string> read_lines(const fs::path& filepath)
{
    assert(fs::is_regular_file(filepath));

    std::ifstream infile(filepath.c_str());

    std::vector<std::string> contents;

    std::string line;
    while (std::getline(infile, line)) {
        contents.emplace_back(std::move(line));
    }

    LOG(Debug) << "Read file from disk: " << filepath;

    return contents;
}

static int validate_path(const std::string& path_to_validate,
                         std::string& validated_path_buffer)
{
    const fs::path canonical_path = fs::canonical(path_to_validate);

    if (!fs::exists(canonical_path)) {
        return -1;
    }

    if (!(fs::is_directory(canonical_path) || fs::is_regular_file(canonical_path))) {
        return -2;
    }

    validated_path_buffer = canonical_path.string();

    return 0;
}

namespace lldbg {

std::map<size_t, std::string> FileHandle::s_filepath_cache;
std::map<size_t, std::string> FileHandle::s_filename_cache;
std::map<size_t, std::vector<std::string>> FileHandle::s_contents_cache;

std::optional<FileHandle> FileHandle::create(const std::string& filepath)
{
    const fs::path canonical_path = fs::canonical(filepath);

    if (!fs::exists(canonical_path)) {
        return {};
    }

    if (!fs::is_regular_file(canonical_path)) {
        return {};
    }

    if (!canonical_path.has_filename()) {
        return {};
    }

    const size_t path_hash = fs::hash_value(canonical_path);

    {
        auto it = s_filepath_cache.find(path_hash);
        if (it == s_filepath_cache.end()) {
            s_filepath_cache.emplace(path_hash, canonical_path.string());
        }
    }

    {
        auto it = s_filename_cache.find(path_hash);
        if (it == s_filename_cache.end()) {
            s_filename_cache[path_hash] = canonical_path.filename().string();
        }
    }

    {
        auto it = s_contents_cache.find(path_hash);
        if (it == s_contents_cache.end()) {
            s_contents_cache[path_hash] = read_lines(canonical_path);
        }
    }

    return FileHandle(path_hash);
}

std::unique_ptr<FileBrowserNode> FileBrowserNode::create(const fs::path& relative_path)
{
    if (!fs::exists(relative_path)) {
        return nullptr;
    }

    const fs::path canonical_path = fs::canonical(relative_path);

    if (!fs::is_directory(canonical_path) && !fs::is_regular_file(canonical_path)) {
        LOG(Error) << "Attemped to load a path (" << canonical_path
                   << ") that wasn't a directory or regular file!";
        return nullptr;
    }

    if (!canonical_path.has_filename()) {
        LOG(Error) << "No filename for file: " << canonical_path;
        return nullptr;
    }

    return std::unique_ptr<FileBrowserNode>(new FileBrowserNode(canonical_path));
}

std::unique_ptr<FileBrowserNode> FileBrowserNode::create(const char* relative_path)
{
    return FileBrowserNode::create(fs::path(relative_path));
}

void FileBrowserNode::open_children()
{
    if (!m_already_opened) {
        m_already_opened = true;

        for (const fs::path& p : fs::directory_iterator(m_path)) {
            std::unique_ptr<FileBrowserNode> new_child_node = FileBrowserNode::create(p);
            if (new_child_node) {
                this->children.emplace_back(std::move(new_child_node));
            }
        }

        std::sort(this->children.begin(), this->children.end(),
                  [](const std::unique_ptr<FileBrowserNode>& a,
                     const std::unique_ptr<FileBrowserNode>& b) {
                      if (a->is_directory() && !b->is_directory()) {
                          return true;
                      }
                      else if (!a->is_directory() && b->is_directory()) {
                          return false;
                      }
                      else {
                          return strcmp(a->filename(), b->filename()) < 0;
                      }
                  });
    }
}

const std::variant<FileReadError, FileReference> OpenFiles::read_from_disk(
    const fs::path& canonical_path)
{
    const std::string canonical_path_str = canonical_path.string();

    const auto old_it = m_cache.find(canonical_path_str);

    if (old_it == m_cache.end()) {  // file hasn't been cached already
        if (!fs::exists(canonical_path)) {
            return FileReadError::DoesNotExist;
        }

        if (!fs::is_regular_file(canonical_path)) {
            return FileReadError::NotRegularFile;
        }

        auto new_it = m_cache.emplace(canonical_path_str, read_lines(canonical_path));

        std::vector<std::string>* new_contents = &new_it.first->second;

        return FileReference(new_contents, canonical_path);
    }
    else {  // found the file in the cache
        return FileReference(std::addressof(old_it->second), canonical_path);
    }
}

bool OpenFilesNew::open(const std::string& requested_filepath)
{
    const auto handle_attempt = FileHandle::create(requested_filepath);

    if (!handle_attempt) return false;

    const FileHandle handle = *handle_attempt;

    {
        auto it = std::find(m_files.begin(), m_files.end(), handle);
        if (it != m_files.end()) {
            m_focus = it - m_files.begin();
            LOG(Verbose) << "Successfully switched focus to already-opened file: "
                         << requested_filepath;
            return true;
        }
    }

    m_files.push_back(handle);
    m_focus = m_files.size() - 1;

    LOG(Verbose) << "Successfully opened new file: " << requested_filepath;
    LOG(Debug) << "Number of currently open files: " << m_files.size();

    return true;
}

bool OpenFiles::open(const std::string& requested_filepath)
{
    const fs::path canonical_path = fs::canonical(requested_filepath);

    auto it = std::find_if(m_refs.begin(), m_refs.end(), [&](const FileReference& ref) {
        return ref.canonical_path == canonical_path;
    });

    if (it != m_refs.end()) {  // already in set
        m_focus = it - m_refs.begin();
        return true;
    }

    return MATCH(
        read_from_disk(canonical_path),

        [&](const FileReadError& error) {
            switch (error) {
                case FileReadError::DoesNotExist:
                    LOG(Warning)
                        << "Attempted to open non-existent file: " << requested_filepath;
                    break;
                case FileReadError::NotRegularFile:
                    LOG(Warning) << "Attempted to open something other than a regular file "
                                    "(maybe a directory?): "
                                 << requested_filepath;
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
        });
}

void OpenFilesNew::close(size_t tab_index)
{
    m_files.erase(m_files.begin() + tab_index);

    if (m_files.empty()) {
        m_focus = {};
    }
    else {
        assert(m_focus);
        const size_t old_open_file_count = m_files.size();
        const size_t old_focused_tab_idx = *m_focus;

        const bool closed_tab_to_left_of_focus = tab_index < old_focused_tab_idx;
        const bool closed_last_tab =
            tab_index == old_focused_tab_idx && old_focused_tab_idx == old_open_file_count - 1;

        if (closed_tab_to_left_of_focus || closed_last_tab) {
            m_focus = old_focused_tab_idx - 1;
        }
    }
}

void OpenFiles::close(const std::string& filepath)
{
    const fs::path path_to_close = fs::canonical(filepath);

    for_each_open_file([path_to_close](const FileReference& ref, bool) -> Action {
        if (ref.canonical_path == path_to_close) {
            return Action::Close;
        }
        else {
            return Action::Nothing;
        }
    });
}

void BreakPointSetNew::synchronize(lldb::SBTarget target)
{
    for (auto& [_, bps] : m_cache) bps.clear();

    for (uint32_t i = 0; i < target.GetNumBreakpoints(); i++) {
        lldb::SBBreakpoint bp = target.GetBreakpointAtIndex(i);
        lldb::SBBreakpointLocation location = bp.GetLocationAtIndex(0);

        if (!location.IsValid()) {
            LOG(Error) << "Invalid breakpoint location encountered by LLDB.";
        }

        lldb::SBAddress address = location.GetAddress();

        if (!address.IsValid()) {
            LOG(Error) << "Invalid lldb::SBAddress for breakpoint encountered by LLDB.";
        }

        lldb::SBLineEntry line_entry = address.GetLineEntry();

        const std::string bp_filepath =
            fmt::format("{}/{}", line_entry.GetFileSpec().GetDirectory(),
                        line_entry.GetFileSpec().GetFilename());

        const auto maybe_handle = FileHandle::create(bp_filepath);
        if (!maybe_handle) {
            LOG(Error) << "Invalid filepath found for breakpoint: " << bp_filepath;
            continue;
        }

        const FileHandle handle = *maybe_handle;

        auto it = m_cache.find(handle);

        if (it == m_cache.end()) {
            m_cache.emplace(handle, std::unordered_set<int>({(int)line_entry.GetLine()}));
        }
        else {
            it->second.insert((int)line_entry.GetLine());
        }
    }
}

void BreakPointSet::Synchronize(lldb::SBTarget target)
{
    m_cache.clear();

    for (uint32_t i = 0; i < target.GetNumBreakpoints(); i++) {
        lldb::SBBreakpoint bp = target.GetBreakpointAtIndex(i);
        lldb::SBBreakpointLocation location = bp.GetLocationAtIndex(0);

        if (!location.IsValid()) {
            LOG(Error)
                << "BreakPointSet::Synchronize :: Invalid breakpoint location encountered!";
        }

        lldb::SBAddress address = location.GetAddress();

        if (!address.IsValid()) {
            LOG(Error)
                << "BreakPointSet::Synchronize :: Invalid lldb::SBAddress for breakpoint!";
        }

        lldb::SBLineEntry line_entry = address.GetLineEntry();

        std::string full_path;
        full_path += build_string(line_entry.GetFileSpec().GetDirectory());
        full_path += "/";
        full_path += build_string(line_entry.GetFileSpec().GetFilename());

        const std::string canonical_path = fs::canonical(full_path).string();

        auto it = m_cache.find(canonical_path);

        if (it == m_cache.end()) {
            m_cache[canonical_path] = {(int)line_entry.GetLine()};
        }
        else {
            it->second.insert(line_entry.GetLine());
        }
    }
}

// void BreakPointSetNew::add(FileHandle handle, int line)
// {
//     auto it = m_cache.find(handle);
//
//     if (it == m_cache.end()) {
//         m_cache.emplace(handle, {line});
//     }
//     else {
//         it->second.insert(line);
//     }
// }

void BreakPointSet::Add(const std::string& file, int line)
{
    // TODO: implement universal FileHandle type to avoid dealing with paths as much as
    // possible
    static std::string canonical_path;
    const int ret_code = validate_path(file, canonical_path);

    if (ret_code != 0) {
        LOG(Error) << "Attempted to add breakpoint to invalid file: " << file;
        return;
    }

    auto it = m_cache.find(canonical_path);

    if (it == m_cache.end()) {
        m_cache[canonical_path] = {line};
    }
    else {
        std::unordered_set<int>& breakpoints = it->second;
        breakpoints.insert(line);
    }
}

// void BreakPointSetNew::remove(FileHandle handle, int line)
// {
//     auto it = m_cache.find(canonical_path);
//
//     if (it == m_cache.end()) {
//         LOG(Error) << "Attempted to remove breakpoint from file with no recorded
//         breakpoints: "
//                    << file;
//     }
//     else {
//         it->second.erase(line);
//     }
// }

void BreakPointSet::Remove(const std::string& file, int line)
{
    static std::string canonical_path;
    const int ret_code = validate_path(file, canonical_path);

    if (ret_code != 0) {
        LOG(Error)
            << "BreakPointSet::Remove :: Attempted to remove breakpoint from invalid file: "
            << file;
        return;
    }

    auto it = m_cache.find(canonical_path);

    if (it == m_cache.end()) {
        LOG(Debug) << "Attempted to remove breakpoint from file with no recorded breakpoints: "
                   << file;
    }
    else {
        std::unordered_set<int>& breakpoints = it->second;
        breakpoints.erase(line);
    }
}

const std::unordered_set<int> BreakPointSet::Get(const std::string& file)
{
    static std::string canonical_path;
    const int ret_code = validate_path(file, canonical_path);

    if (ret_code != 0) {
        LOG(Error) << "BreakPointSet::Get :: Attempted to get breakpoints for invalid file: "
                   << file;
        return std::unordered_set<int>();
    }

    auto it = m_cache.find(canonical_path);

    if (it == m_cache.end()) {
        return std::unordered_set<int>();
    }
    else {
        return it->second;
    }
}

const std::unordered_set<int>* BreakPointSetNew::Get(FileHandle handle)
{
    auto it = m_cache.find(handle);

    if (it == m_cache.end()) {
        return nullptr;
    }
    else {
        return std::addressof(it->second);
    }
}

}  // namespace lldbg
