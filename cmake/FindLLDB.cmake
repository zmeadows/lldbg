# File: cmake/FindLLDB.cmake
# Fallback finder for LLDB when official CONFIG package isn't available.
# Exposes imported target: lldbg::lldb
# Tries macOS framework first, then generic headers+library search.

include(FindPackageHandleStandardArgs)

# macOS: LLDB.framework (from Xcode / CLT)
if(APPLE)
  # Determine the selected Developer dir (works with full Xcode and CLT-only)
  execute_process(COMMAND xcode-select -p OUTPUT_VARIABLE _DEVELOPER_DIR
                  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)

  # Candidate locations across recent Xcode images:
  # - Xcode 15/16: Developer/Library/PrivateFrameworks
  # - Older:       SharedFrameworks or Toolchains/.../Library/Frameworks
  # - CLT-only:    /Library/Developer/CommandLineTools/Library/(Private)Frameworks
  set(_LLDB_FRAMEWORK_HINTS
      "/Applications/Xcode.app/Contents/Developer/Library/PrivateFrameworks"
      "/Applications/Xcode.app/Contents/SharedFrameworks"
      "${_DEVELOPER_DIR}/Library/PrivateFrameworks"
      "${_DEVELOPER_DIR}/../SharedFrameworks"
      "${_DEVELOPER_DIR}/Toolchains/XcodeDefault.xctoolchain/Library/Frameworks"
      "/Library/Developer/CommandLineTools/Library/PrivateFrameworks"
      "/Library/Developer/CommandLineTools/Library/Frameworks")

  # Also honor user hints if provided
  if(DEFINED LLDB_DIR)
    list(APPEND _LLDB_FRAMEWORK_HINTS "${LLDB_DIR}")
  endif()
  if(DEFINED LLVM_DIR)
    list(APPEND _LLDB_FRAMEWORK_HINTS "${LLVM_DIR}")
  endif()

  # find_library returns the .framework bundle path for frameworks
  find_library(
    LLDB_FRAMEWORK
    NAMES LLDB
    PATHS ${_LLDB_FRAMEWORK_HINTS}
    PATH_SUFFIXES "" LLDB.framework
    NO_DEFAULT_PATH)

  if(LLDB_FRAMEWORK)
    set(LLDB_LIBRARY "${LLDB_FRAMEWORK}")
    # Handle both flat and versioned framework layouts
    if(EXISTS "${LLDB_FRAMEWORK}/Headers")
      set(LLDB_INCLUDE_DIR "${LLDB_FRAMEWORK}/Headers")
    elseif(EXISTS "${LLDB_FRAMEWORK}/Versions/A/Headers")
      set(LLDB_INCLUDE_DIR "${LLDB_FRAMEWORK}/Versions/A/Headers")
    endif()
  endif()
endif()

if(NOT LLDB_FOUND)
  # Generic search (Linux, BSD; also works for non-framework macOS installs)
  # Allow user hints via standard cache vars:
  #   -DLLDB_DIR=... or -DLLVM_DIR=... (we look around these for includes/libs)
  set(_LLDB_HINTS "")
  if(DEFINED LLDB_DIR)
    list(APPEND _LLDB_HINTS "${LLDB_DIR}")
  endif()
  if(DEFINED LLVM_DIR)
    list(APPEND _LLDB_HINTS "${LLVM_DIR}")
  endif()

  # Likely include locations across platforms
  find_path(
    LLDB_INCLUDE_DIR
    NAMES lldb/API/LLDB.h
    HINTS ${_LLDB_HINTS}
    PATH_SUFFIXES
      include
      include/lldb
      ../include
      ../include/lldb
    PATHS /usr/include
          /usr/local/include
          /usr/lib/llvm-19/include
          /usr/lib/llvm-18/include
          /usr/lib/llvm-17/include
          /opt/homebrew/opt/llvm/include)

  # Likely library locations (names vary by distro/version)
  find_library(
    LLDB_LIBRARY
    NAMES lldb
          liblldb
          lldb-19
          lldb-18
          lldb-17
    HINTS ${_LLDB_HINTS}
    PATH_SUFFIXES lib lib64
    PATHS /usr/lib
          /usr/local/lib
          /usr/lib64
          /usr/lib/llvm-19/lib
          /usr/lib/llvm-18/lib
          /usr/lib/llvm-17/lib
          /opt/homebrew/opt/llvm/lib)

  find_package_handle_standard_args(LLDB REQUIRED_VARS LLDB_LIBRARY
                                                       LLDB_INCLUDE_DIR)

  if(LLDB_FOUND AND NOT TARGET lldbg::lldb)
    add_library(lldbg::lldb INTERFACE IMPORTED)
    set_target_properties(
      lldbg::lldb
      PROPERTIES INTERFACE_LINK_LIBRARIES "${LLDB_LIBRARY}"
                 INTERFACE_INCLUDE_DIRECTORIES "${LLDB_INCLUDE_DIR}")
  endif()
endif()
