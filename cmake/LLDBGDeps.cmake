# Central dependency resolution for lldbg.
# Prefers system packages;
#   falls back to vendored FetchContent with pinned versions.

option(LLDBG_USE_SYSTEM_DEPS "Prefer system packages over vendored sources" ON)

# ---- Version pins (adjust here) ----------------------------------------------
set(LLDBG_PIN_FMT "9.1.0")
set(LLDBG_PIN_GLFW "3.3.8")
set(LLDBG_PIN_IMGUIFILEDIALOG "v0.6.8")
set(LLDBG_PIN_CXXOPTS "v3.1.1")

set(LLDBG_PIN_IMGUI_TAG "v1.90.9")
set(LLDBG_PIN_IMGUI_URL
    "https://github.com/ocornut/imgui/archive/refs/tags/${LLDBG_PIN_IMGUI_TAG}.tar.gz"
)

include(FetchContent)

# Use DOWNLOAD_EXTRACT_TIMESTAMP only on CMake >= 3.24
macro(lldbg_fetchcontent_declare name)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
    FetchContent_Declare(${name} ${ARGN} DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  else()
    FetchContent_Declare(${name} ${ARGN})
  endif()
endmacro()

# ---- fmt --------------------------------------------------------------------
if(LLDBG_USE_SYSTEM_DEPS)
  find_package(fmt CONFIG QUIET) # accepts 6.x..10.x
endif()
if(NOT TARGET fmt::fmt)
  lldbg_fetchcontent_declare(
    fmt GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG ${LLDBG_PIN_FMT})
  FetchContent_MakeAvailable(fmt)
  target_compile_options(
    fmt PRIVATE $<$<CXX_COMPILER_ID:Clang,AppleClang>:-w>
                $<$<CXX_COMPILER_ID:GNU>:-w> $<$<CXX_COMPILER_ID:MSVC>:/w>)
endif()

# ---- GLFW -------------------------------------------------------------------
if(LLDBG_USE_SYSTEM_DEPS)
  find_package(glfw3 QUIET CONFIG) # Debian/Ubuntu export a config
  if(NOT TARGET glfw) # some distros export glfw::glfw only
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND AND NOT TARGET glfw)
      pkg_check_modules(
        GLFW3
        QUIET
        IMPORTED_TARGET
        glfw3)
      if(TARGET PkgConfig::GLFW3)
        add_library(glfw INTERFACE
        )# alias target so the rest of the build can link 'glfw'
        target_link_libraries(glfw INTERFACE PkgConfig::GLFW3)
      endif()
    endif()
  endif()
endif()
if(NOT TARGET glfw)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
  lldbg_fetchcontent_declare(
    glfw GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG ${LLDBG_PIN_GLFW})
  FetchContent_MakeAvailable(glfw)
endif()

# ------------------------- OpenGL --------------------------------------------
set(OpenGL_GL_PREFERENCE "GLVND")
find_package(OpenGL REQUIRED) # provides OpenGL::GL

# ---- Dear ImGui -------------------------------------------------------------
if(LLDBG_USE_SYSTEM_DEPS)
  find_package(imgui QUIET) # Debian/Ubuntu libimgui-dev
  # Normalize a plain 'imgui' target if the
  # package doesn't export 'imgui::imgui'
  if(TARGET imgui AND NOT TARGET imgui::imgui)
    add_library(imgui::imgui ALIAS imgui)
  endif()
endif()

if(NOT TARGET imgui::imgui)
  message(STATUS "[deps] Fetching Dear ImGui")
  lldbg_fetchcontent_declare(
    imgui
    URL ${LLDBG_PIN_IMGUI_URL}
        # Keep fetch fast & reproducible
        GIT_SHALLOW
        FALSE
        UPDATE_DISCONNECTED
        TRUE)
  FetchContent_MakeAvailable(imgui)

  # Resolve ImGui root (handles GitHub archive with versioned subdir)
  set(_IMGUI_DIR "${imgui_SOURCE_DIR}")
  if(NOT EXISTS "${_IMGUI_DIR}/imgui.cpp")
    file(GLOB _IMGUI_SUBDIRS LIST_DIRECTORIES TRUE "${imgui_SOURCE_DIR}/*")
    foreach(_d IN LISTS _IMGUI_SUBDIRS)
      if(EXISTS "${_d}/imgui.cpp")
        set(_IMGUI_DIR "${_d}")
        break()
      endif()
    endforeach()
  endif()
  message(STATUS "[deps] ImGui root: ${_IMGUI_DIR}")

  # Build ImGui as a proper target with GLFW + OpenGL3 backends
  add_library(imgui)

  # Core & backend sources
  set(IMGUI_SOURCES
      "${_IMGUI_DIR}/imgui.cpp"
      "${_IMGUI_DIR}/imgui_draw.cpp"
      "${_IMGUI_DIR}/imgui_tables.cpp"
      "${_IMGUI_DIR}/imgui_widgets.cpp"
      "${_IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
      "${_IMGUI_DIR}/backends/imgui_impl_opengl3.cpp")

  # Optional demo
  if(LLDBG_WITH_IMGUI_DEMO)
    if(EXISTS "${_IMGUI_DIR}/imgui_demo.cpp")
      list(APPEND IMGUI_SOURCES "${_IMGUI_DIR}/imgui_demo.cpp")
    else()
      message(
        FATAL_ERROR
          "LLDBG_WITH_IMGUI_DEMO=ON but '${_IMGUI_DIR}/imgui_demo.cpp' not found. "
          "Try clearing build/_deps or check the ImGui archive layout.")
    endif()
  endif()

  target_sources(imgui PRIVATE ${IMGUI_SOURCES})

  target_include_directories(imgui SYSTEM PUBLIC "${_IMGUI_DIR}"
                                                 "${_IMGUI_DIR}/backends")

  # Ensure OpenGL target exists if caller hasn't found it yet
  if(NOT TARGET OpenGL::GL)
    find_package(OpenGL QUIET)
  endif()

  target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS
                                          GLFW_INCLUDE_NONE)

  target_link_libraries(imgui PUBLIC glfw OpenGL::GL ${CMAKE_DL_LIBS})

  # QUIET the library's own compilation warnings...
  target_compile_options(
    imgui
    PRIVATE $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-nontrivial-memcall
            -Wno-everything>
            $<$<CXX_COMPILER_ID:GNU>:-w>
            $<$<CXX_COMPILER_ID:MSVC>:/w>)

  add_library(imgui::imgui ALIAS imgui)
endif()

# ---- ImGuiFileDialog (vendored) ---------------------------------------------
lldbg_fetchcontent_declare(
  imguifiledialog
  GIT_REPOSITORY https://github.com/aiekick/ImGuiFileDialog.git
  GIT_TAG ${LLDBG_PIN_IMGUIFILEDIALOG}
          GIT_SHALLOW
          TRUE
          UPDATE_DISCONNECTED
          TRUE)

FetchContent_GetProperties(imguifiledialog)
if(NOT imguifiledialog_POPULATED)
  cmake_policy(PUSH)
  if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD) # allow Populate without MakeAvailable
  endif()
  FetchContent_Populate(imguifiledialog)
  cmake_policy(POP)
endif()

add_library(ImGuiFileDialogLib STATIC
            "${imguifiledialog_SOURCE_DIR}/ImGuiFileDialog.cpp")

target_include_directories(ImGuiFileDialogLib SYSTEM
                           PUBLIC "${imguifiledialog_SOURCE_DIR}")
target_compile_options(
  ImGuiFileDialogLib
  PRIVATE $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-everything>
          $<$<CXX_COMPILER_ID:GNU>:-w> $<$<CXX_COMPILER_ID:MSVC>:/w>)

target_link_libraries(ImGuiFileDialogLib PUBLIC imgui::imgui)
add_library(ImGuiFileDialog::ImGuiFileDialog ALIAS ImGuiFileDialogLib)

# ---- cxxopts (header-only) --------------------------------------------------
if(LLDBG_USE_SYSTEM_DEPS)
  find_package(cxxopts QUIET)
endif()
if(NOT TARGET cxxopts::cxxopts)
  lldbg_fetchcontent_declare(
    cxxopts GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    GIT_TAG ${LLDBG_PIN_CXXOPTS})
  FetchContent_MakeAvailable(cxxopts)
endif()

# ---------- LLDB discovery (prefer official package config) ----------
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)

# Hints for users/distros with multiple LLVMs installed:
set(LLVM_DIR ""
    CACHE PATH
          "Path to LLVMConfig.cmake (e.g. /usr/lib/llvm-18/lib/cmake/llvm)")
set(LLDB_DIR ""
    CACHE PATH
          "Path to LLDBConfig.cmake (e.g. /usr/lib/llvm-18/lib/cmake/lldb)")

# Always prefer official CMake packages if present.
# (Config mode only; we deliberately do not search MODULEs yet.)
set(_lldb_try_config TRUE)

# 1) Try LLDB Config directly
if(_lldb_try_config)
  find_package(LLDB CONFIG QUIET)
endif()

# 2) If LLDB not found, some platforms require LLVM found first
if(NOT LLDB_FOUND AND _lldb_try_config)
  find_package(LLVM CONFIG QUIET)
  find_package(LLDB CONFIG QUIET)
endif()

# 3) If still not found, try to auto-discover a likely prefix and retry Config mode
#    - On any platform: use `llvm-config --cmakedir` if available
#    - On macOS: also try Homebrew prefix (no install, just hint if brew exists)
if(NOT LLDB_FOUND)
  # Try llvm-config (common on Linux; also present if Homebrew llvm installed)
  find_program(_LLVM_CONFIG_EXEC llvm-config QUIET)
  if(_LLVM_CONFIG_EXEC)
    execute_process(
      COMMAND "${_LLVM_CONFIG_EXEC}" --cmakedir OUTPUT_VARIABLE _LLVM_CMAKEDIR
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(_LLVM_CMAKEDIR AND EXISTS "${_LLVM_CMAKEDIR}")
      list(APPEND CMAKE_PREFIX_PATH "${_LLVM_CMAKEDIR}")
    endif()
  endif()

  # Try Homebrew prefix on macOS (ARM: /opt/homebrew, Intel: /usr/local)
  if(APPLE)
    find_program(_BREW_EXEC brew QUIET)
    if(_BREW_EXEC)
      execute_process(
        COMMAND "${_BREW_EXEC}" --prefix OUTPUT_VARIABLE _BREW_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
      if(_BREW_PREFIX AND EXISTS "${_BREW_PREFIX}")
        # Add the prefix so CMake can discover ${prefix}/lib/cmake/{lldb,llvm}
        list(APPEND CMAKE_PREFIX_PATH "${_BREW_PREFIX}")
      endif()
    endif()
  endif()

  # Retry Config mode after adding any discovered prefixes
  find_package(LLDB CONFIG QUIET)
  if(NOT LLDB_FOUND)
    find_package(LLVM CONFIG QUIET)
    find_package(LLDB CONFIG QUIET)
  endif()
endif()

# 4) Fallback to our MODULE search (supports macOS framework + Linux libs)
#    This uses cmake/FindLLDB.cmake which should handle:
#      - macOS LLDB.framework + toolchain headers
#      - Linux liblldb + headers
if(NOT LLDB_FOUND)
  find_package(LLDB MODULE REQUIRED)
endif()

# 5) Normalize to a single target name for the rest of the project
if(TARGET LLDB::LLDB)
  add_library(lldbg::lldb ALIAS LLDB::LLDB)
elseif(TARGET LLDB::liblldb)
  add_library(lldbg::lldb ALIAS LLDB::liblldb)
elseif(TARGET lldb)
  add_library(lldbg::lldb ALIAS lldb)
elseif(TARGET LLDB)
  add_library(lldbg::lldb ALIAS LLDB)
elseif(TARGET lldbg::lldb)
  # Provided by our FindLLDB.cmake fallback
else()
  message(
    FATAL_ERROR
      "LLDB was not discovered.\n"
      "Tips:\n"
      "  • Provide LLDB/LLVM CMake packages via CMAKE_PREFIX_PATH or LLDB_DIR/LLVM_DIR.\n"
      "  • macOS: `brew install llvm` then set CMAKE_PREFIX_PATH to `$(brew --prefix llvm)` or just `$(brew --prefix)`.\n"
      "  • Linux: install the lldb development package for your distro, or build/install LLVM+LLDB.\n"
  )
endif()

# Status: show where LLDB came from (helpful for local builds)
get_target_property(_LLDBG_LLDB_INCS lldbg::lldb INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(_LLDBG_LLDB_LIBS lldbg::lldb INTERFACE_LINK_LIBRARIES)
message(STATUS "LLDBG: Using lldbg::lldb")
message(STATUS "       includes: ${_LLDBG_LLDB_INCS}")
message(STATUS "       link libs: ${_LLDBG_LLDB_LIBS}")
