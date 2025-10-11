include(FetchContent)

macro(lldbg_fetchcontent_declare name)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
    FetchContent_Declare(${name} ${ARGN} DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  else()
    FetchContent_Declare(${name} ${ARGN})
  endif()
endmacro()

option(LLDBG_WITH_IMGUI_DEMO "Build ImGui with demo window" OFF)

# ------------------------- fmt ------------------------------------------------
# Try system fmt; fallback to FetchContent
find_package(fmt CONFIG QUIET)
if(NOT fmt_FOUND)
  message(STATUS "[deps] Fetching fmt")
  lldbg_fetchcontent_declare(
    fmt URL https://github.com/fmtlib/fmt/archive/refs/tags/11.0.2.tar.gz)
  FetchContent_MakeAvailable(fmt)
endif()

# ------------------------- GLFW ----------------------------------------------
# Many distros export a `glfw` target, some export `glfw3`. Normalize to `glfw`.
find_package(glfw3 CONFIG QUIET)
if(NOT glfw3_FOUND)
  find_package(glfw3 QUIET) # Some Find modules export plain 'glfw'
endif()

if(TARGET glfw3 AND NOT TARGET glfw)
  add_library(glfw ALIAS glfw3)
endif()

if(TARGET glfw AND NOT TARGET glfw::glfw)
  add_library(glfw::glfw ALIAS glfw)
endif()

if(NOT TARGET glfw)
  message(STATUS "[deps] Fetching GLFW")
  set(GLFW_BUILD_EXAMPLES
      OFF
      CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS
      OFF
      CACHE BOOL "" FORCE)
  set(GLFW_BUILD_DOCS
      OFF
      CACHE BOOL "" FORCE)
  set(GLFW_INSTALL
      OFF
      CACHE BOOL "" FORCE)
  lldbg_fetchcontent_declare(
    glfw URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz)
  FetchContent_MakeAvailable(glfw) # defines 'glfw' target
endif()

# ------------------------- OpenGL --------------------------------------------
set(OpenGL_GL_PREFERENCE "GLVND")
find_package(OpenGL REQUIRED) # provides OpenGL::GL

# ------------------------- GLEW ----------------------------------------------
# Prefer a packaged GLEW; otherwise, fetch a CMake-friendly fork.
find_package(GLEW QUIET)
# If a package exports different names, normalize to GLEW::GLEW
if(TARGET glew_s AND NOT TARGET GLEW::GLEW)
  add_library(GLEW::GLEW ALIAS glew_s)
elseif(TARGET libglew_static AND NOT TARGET GLEW::GLEW)
  add_library(GLEW::GLEW ALIAS libglew_static)
endif()

if(NOT TARGET GLEW::GLEW)
  message(STATUS "[deps] Fetching GLEW (cmake wrapper)")
  # Use the CMake-ified wrapper; exports glew_s/libglew_static
  lldbg_fetchcontent_declare(
    glew
    GIT_REPOSITORY
    https://github.com/Perlmint/glew-cmake.git
    GIT_TAG
    b51c5193091d48203da8b6e1ed70e6d468bcb532 # <- pinned commit
    GIT_SHALLOW
    TRUE
    GIT_PROGRESS
    TRUE
    UPDATE_DISCONNECTED
    TRUE)

  set(BUILD_UTILS
      OFF
      CACHE BOOL "" FORCE)
  set(glew-cmake_BUILD_SHARED
      OFF
      CACHE BOOL "" FORCE)
  set(glew-cmake_BUILD_STATIC
      ON
      CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(glew)
  if(TARGET glew_s AND NOT TARGET GLEW::GLEW)
    add_library(GLEW::GLEW ALIAS glew_s)
  elseif(TARGET libglew_static AND NOT TARGET GLEW::GLEW)
    add_library(GLEW::GLEW ALIAS libglew_static)
  endif()
  set(GLEW_LIBRARIES
      GLEW::GLEW
      CACHE STRING "GLEW link shim" FORCE)
  unset(GLEW_LIBRARY CACHE)
  unset(GLEW_LIBRARY_DEBUG CACHE)
  unset(GLEW_LIBRARY_RELEASE CACHE)
  unset(GLEW_INCLUDE_DIR CACHE)
  unset(GLEW_INCLUDE_DIRS CACHE)
endif()

# ------------------------- cxxopts -------------------------------------------
find_package(cxxopts QUIET CONFIG)
if(NOT TARGET cxxopts::cxxopts)
  message(STATUS "[deps] Fetching cxxopts")
  lldbg_fetchcontent_declare(
    cxxopts URL
    https://github.com/jarro2783/cxxopts/archive/refs/tags/v3.2.0.tar.gz)
  FetchContent_MakeAvailable(cxxopts) # defines cxxopts::cxxopts
endif()

# ------------------------- Dear ImGui ----------------------------------------
# Build ImGui as a proper target with the GLFW + OpenGL3 backends used by this
# app.
if(NOT TARGET imgui::imgui)
  message(STATUS "[deps] Fetching Dear ImGui")
  lldbg_fetchcontent_declare(
    imgui URL https://github.com/ocornut/imgui/archive/refs/tags/v1.90.9.tar.gz)
  FetchContent_MakeAvailable(imgui)

  # After FetchContent_MakeAvailable(imgui)
  # Resolve ImGui root (handles GitHub archive with versioned subdir)
  set(_IMGUI_DIR "${imgui_SOURCE_DIR}")
  if(NOT EXISTS "${_IMGUI_DIR}/imgui.cpp")
    file(
      GLOB _IMGUI_SUBDIRS
      LIST_DIRECTORIES TRUE
      "${imgui_SOURCE_DIR}/*")
    foreach(_d IN LISTS _IMGUI_SUBDIRS)
      if(EXISTS "${_d}/imgui.cpp")
        set(_IMGUI_DIR "${_d}")
        break()
      endif()
    endforeach()
  endif()
  message(STATUS "[deps] ImGui root: ${_IMGUI_DIR}")

  add_library(imgui)

  # Core ImGui sources
  set(IMGUI_SOURCES
      "${_IMGUI_DIR}/imgui.cpp"
      "${_IMGUI_DIR}/imgui_draw.cpp"
      "${_IMGUI_DIR}/imgui_tables.cpp"
      "${_IMGUI_DIR}/imgui_widgets.cpp"
      "${_IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
      "${_IMGUI_DIR}/backends/imgui_impl_opengl3.cpp")

  # Add demo source only when requested, and only if it actually exists
  if(LLDBG_WITH_IMGUI_DEMO)
    if(EXISTS "${_IMGUI_DIR}/imgui_demo.cpp")
      list(APPEND IMGUI_SOURCES "${_IMGUI_DIR}/imgui_demo.cpp")
    else()
      message(
        FATAL_ERROR
          "LLDBG_WITH_IMGUI_DEMO=ON but '${_IMGUI_DIR}/imgui_demo.cpp"
          "not found. Try clearing build/_deps or check the"
          " ImGui archive layout.")
    endif()
  endif()

  target_sources(imgui PRIVATE ${IMGUI_SOURCES})

  target_include_directories(imgui SYSTEM PUBLIC "${_IMGUI_DIR}"
                                                 "${_IMGUI_DIR}/backends")

  target_link_libraries(imgui PUBLIC glfw OpenGL::GL)
  target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)

  # QUIET the library's own compilation warnings (Clang/GCC/MSVC)
  target_compile_options(
    imgui
    PRIVATE $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-nontrivial-memcall
            -Wno-everything> $<$<CXX_COMPILER_ID:GNU>:-w>
            $<$<CXX_COMPILER_ID:MSVC>:/w>)

  add_library(imgui::imgui ALIAS imgui)
endif()

# ---------------- ImGuiFileDialog (compile-in, no upstream CMake) -------------
lldbg_fetchcontent_declare(
  imguifiledialog
  GIT_REPOSITORY
  https://github.com/aiekick/ImGuiFileDialog.git
  GIT_TAG
  v0.6.8
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

# ---------- LLDB discovery (prefer official package config) ----------
# Hints for users/distros with multiple LLVMs installed:
set(LLVM_DIR
    ""
    CACHE PATH
          "Path to LLVMConfig.cmake (e.g. /usr/lib/llvm-18/lib/cmake/llvm)")
set(LLDB_DIR
    ""
    CACHE PATH
          "Path to LLDBConfig.cmake (e.g. /usr/lib/llvm-18/lib/cmake/lldb)")

# 1) Try official config package first
find_package(LLDB CONFIG QUIET)
if(NOT LLDB_FOUND)
  # Some platforms require LLVM to be found first
  find_package(LLVM CONFIG QUIET)
  find_package(LLDB CONFIG QUIET)
endif()

# 2) Fallback to our module search (supports macOS framework + Linux libs)
if(NOT LLDB_FOUND)
  find_package(LLDB MODULE REQUIRED)
endif()

# 3) Normalize to a single target name for the rest of the project
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
  message(FATAL_ERROR "LLDB found but no known imported target to alias.")
endif()
