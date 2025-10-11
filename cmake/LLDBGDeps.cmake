# cmake/LLDBGDeps.cmake
# Central dependency resolution for lldbg.
# Prefers system packages;
#   falls back to vendored FetchContent with pinned versions.

option(LLDBG_USE_SYSTEM_DEPS "Prefer system packages over vendored sources" ON)

# ---- Version pins (adjust here) ----------------------------------------------
set(LLDBG_PIN_FMT "9.1.0")
set(LLDBG_PIN_GLFW "3.3.8")
set(LLDBG_PIN_GLEW "b51c5193091d48203da8b6e1ed70e6d468bcb532")
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

# ---- GLEW -------------------------------------------------------------------
if(LLDBG_USE_SYSTEM_DEPS)
  # Prefer a packaged GLEW; normalize to GLEW::GLEW if possible
  find_package(GLEW QUIET)

  # Case 1: package already exports GLEW::GLEW (Config package)
  if(TARGET GLEW::GLEW)
    set(_lldbg_have_glew_target TRUE)

    # Case 2: some packages export glew_s or libglew_static
  elseif(TARGET glew_s)
    add_library(GLEW::GLEW ALIAS glew_s)
    set(_lldbg_have_glew_target TRUE)
  elseif(TARGET libglew_static)
    add_library(GLEW::GLEW ALIAS libglew_static)
    set(_lldbg_have_glew_target TRUE)

    # Case 3: classic FindGLEW with only variables; wrap into an imported target
  elseif(
    GLEW_FOUND
    OR GLEW_LIBRARIES
    OR GLEW_LIBRARY
    OR GLEW_LIBRARY_RELEASE)
    set(_GLEW_LIBS "${GLEW_LIBRARIES}")
    if(NOT _GLEW_LIBS)
      if(GLEW_LIBRARY_RELEASE)
        set(_GLEW_LIBS "${GLEW_LIBRARY_RELEASE}")
      elseif(GLEW_LIBRARY)
        set(_GLEW_LIBS "${GLEW_LIBRARY}")
      endif()
    endif()
    add_library(GLEW::GLEW INTERFACE IMPORTED)
    if(_GLEW_LIBS)
      set_target_properties(GLEW::GLEW PROPERTIES INTERFACE_LINK_LIBRARIES
                                                  "${_GLEW_LIBS}")
    endif()
    if(GLEW_INCLUDE_DIRS)
      set_target_properties(GLEW::GLEW PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                                  "${GLEW_INCLUDE_DIRS}")
    elseif(GLEW_INCLUDE_DIR)
      set_target_properties(GLEW::GLEW PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                                  "${GLEW_INCLUDE_DIR}")
    endif()
    set(_lldbg_have_glew_target TRUE)
  endif()
endif()

if(NOT TARGET GLEW::GLEW)
  message(STATUS "[deps] Fetching GLEW (cmake wrapper)")
  # Use the CMake-ified wrapper; exports glew_s/libglew_static
  lldbg_fetchcontent_declare(
    glew
    GIT_REPOSITORY https://github.com/Perlmint/glew-cmake.git
    GIT_TAG ${LLDBG_PIN_GLEW_CMAKE_COMMIT}
            GIT_SHALLOW
            TRUE
            GIT_PROGRESS
            TRUE
            UPDATE_DISCONNECTED
            TRUE)

  set(BUILD_UTILS OFF CACHE BOOL "" FORCE)
  set(glew-cmake_BUILD_SHARED OFF CACHE BOOL "" FORCE)
  set(glew-cmake_BUILD_STATIC ON CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(glew)

  if(TARGET glew_s AND NOT TARGET GLEW::GLEW)
    add_library(GLEW::GLEW ALIAS glew_s)
  elseif(TARGET libglew_static AND NOT TARGET GLEW::GLEW)
    add_library(GLEW::GLEW ALIAS libglew_static)
  endif()

  # Provide a consistent variable for any downstream scripts that expect it
  set(GLEW_LIBRARIES GLEW::GLEW CACHE STRING "GLEW link shim" FORCE)

  # Clean up FindGLEW cache vars that can confuse future runs
  unset(GLEW_LIBRARY CACHE)
  unset(GLEW_LIBRARY_DEBUG CACHE)
  unset(GLEW_LIBRARY_RELEASE CACHE)
  unset(GLEW_INCLUDE_DIR CACHE)
  unset(GLEW_INCLUDE_DIRS CACHE)
endif()

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

  target_link_libraries(imgui PUBLIC glfw OpenGL::GL ${CMAKE_DL_LIBS})
  target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)

  # QUIET the library's own compilation warnings (Clang/GCC/MSVC)
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
# Hints for users/distros with multiple LLVMs installed:
set(LLVM_DIR ""
    CACHE PATH
          "Path to LLVMConfig.cmake (e.g. /usr/lib/llvm-18/lib/cmake/llvm)")
set(LLDB_DIR ""
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
