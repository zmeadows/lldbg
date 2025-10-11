# === File: cmake/LLDBGDeps.cmake =============================================
# Centralized dependency resolution for lldbgui: prefer system packages,
# fallback to FetchContent. LLDB intentionally left as-is for now.

cmake_minimum_required(VERSION 3.16)
include(FetchContent)

option(LLDBG_WITH_IMGUI_DEMO "Build ImGui with demo window" OFF)

# ------------------------- fmt ------------------------------------------------
# Try system fmt; fallback to FetchContent
find_package(fmt CONFIG QUIET)
if(NOT fmt_FOUND)
  message(STATUS "[deps] Fetching fmt")
  FetchContent_Declare(
    fmt URL https://github.com/fmtlib/fmt/archive/refs/tags/11.0.2.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
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
  FetchContent_Declare(
    glfw URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz
             DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
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
  FetchContent_Declare(
    glew
    GIT_REPOSITORY https://github.com/Perlmint/glew-cmake.git
    GIT_TAG b51c5193091d48203da8b6e1ed70e6d468bcb532 # <- pinned commit
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    UPDATE_DISCONNECTED TRUE # don’t “git pull” after first fetch
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

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
  FetchContent_Declare(
    cxxopts
    URL https://github.com/jarro2783/cxxopts/archive/refs/tags/v3.2.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(cxxopts) # defines cxxopts::cxxopts
endif()

# ------------------------- Dear ImGui ----------------------------------------
# Build ImGui as a proper target with the GLFW + OpenGL3 backends used by this
# app.
if(NOT TARGET imgui::imgui)
  message(STATUS "[deps] Fetching Dear ImGui")
  FetchContent_Declare(
    imgui URL https://github.com/ocornut/imgui/archive/refs/tags/v1.90.9.tar.gz
              DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(imgui)
  # existing
  add_library(imgui)
  target_sources(
    imgui
    PRIVATE
      "${imgui_SOURCE_DIR}/imgui.cpp"
      "${imgui_SOURCE_DIR}/imgui_draw.cpp"
      "${imgui_SOURCE_DIR}/imgui_tables.cpp"
      "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
      "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
      "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
      $<$<BOOL:${LLDBG_WITH_IMGUI_DEMO}>:"${imgui_SOURCE_DIR}/imgui_demo.cpp">)

  # mark third-party headers as SYSTEM so they don't warn when included by YOUR code
  target_include_directories(imgui SYSTEM PUBLIC "${imgui_SOURCE_DIR}"
                                                 "${imgui_SOURCE_DIR}/backends")

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
FetchContent_Declare(
  imguifiledialog
  GIT_REPOSITORY https://github.com/aiekick/ImGuiFileDialog.git
  GIT_TAG v0.6.8
  GIT_SHALLOW TRUE
  UPDATE_DISCONNECTED TRUE)

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
