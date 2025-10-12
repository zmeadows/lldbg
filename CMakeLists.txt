cmake_minimum_required(VERSION 3.20...4.1)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(cmake/PreferClang.cmake)

project(
  lldbgui
  VERSION 0.1
  DESCRIPTION "A GUI for lldb."
  LANGUAGES CXX C)

# ---- macOS toolchain hygiene (keep local builds aligned with CI) ----
if(APPLE)
  # Prefer official CMake packages (e.g. LLDB/LLVM) over Find modules.
  set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)

  # Make the target arch explicit if the user didn't specify it.
  if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
      set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Target macOS arch"
                                                FORCE)
    else()
      set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Target macOS arch"
                                                 FORCE)
    endif()
  endif()

  # Enforce AppleClang by default to avoid libc++/SDK ABI mismatches.
  # Allow override via -DLLDBG_ALLOW_NON_APPLECLANG=ON for power users.
  option(LLDBG_ALLOW_NON_APPLECLANG "Allow non-Apple Clang on macOS" OFF)

  execute_process(
    COMMAND "${CMAKE_CXX_COMPILER}" --version OUTPUT_VARIABLE _LLDBG_CXX_VER
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)

  if(NOT LLDBG_ALLOW_NON_APPLECLANG)
    if(NOT
       _LLDBG_CXX_VER
       MATCHES
       "Apple[ ]+clang")
      message(
        FATAL_ERROR
          "On macOS, build with AppleClang to avoid ABI issues.\n"
          "Tip: export CC=\"$(xcrun --find clang)\" CXX=\"$(xcrun --find clang++)\" "
          "or pass -DLLDBG_ALLOW_NON_APPLECLANG=ON if you know what you're doing.\n"
          "Detected compiler: ${CMAKE_CXX_COMPILER} — ${_LLDBG_CXX_VER}")
    endif()
  endif()

  message(
    STATUS "macOS: C++ compiler = ${CMAKE_CXX_COMPILER} — ${_LLDBG_CXX_VER}")
  message(STATUS "macOS: CMAKE_OSX_ARCHITECTURES = ${CMAKE_OSX_ARCHITECTURES}")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

include(cmake/Warnings.cmake)
include(cmake/LLDBGDeps.cmake)

# if(CMAKE_BUILD_TYPE MATCHES "Debug")
#   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEBUG")
#   set(ENV{ASAN_OPTIONS} "detect_container_overflow=0")
#   set(CMAKE_CXX_FLAGS
#       "${CMAKE_CXX_FLAGS} -Werror -fsanitize=undefined -fsanitize=address")
# endif()

file(
  GLOB
  LLDBGUI_SOURCES
  "${CMAKE_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_SOURCE_DIR}/src/*/*.cpp")

# Remove main.cpp from the sources for the library
list(REMOVE_ITEM LLDBGUI_SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp")

add_definitions(-DLLDBG_TESTS_DIR="${CMAKE_SOURCE_DIR}/test/")

# Create a library for lldbgui
add_library(lldbgui_lib ${LLDBGUI_SOURCES})

# if(CMAKE_BUILD_TYPE MATCHES "Debug")
#   target_link_options(
#       lldbgui_lib BEFORE PUBLIC -fsanitize=undefined PUBLIC -fsanitize=address
#   )
# endif()

# Create the lldbgui executable with main.cpp
add_executable(lldbgui ${CMAKE_SOURCE_DIR}/src/main.cpp)
target_link_libraries(lldbgui PRIVATE lldbgui_lib)

# Enable exports for the library
set_target_properties(lldbgui_lib PROPERTIES ENABLE_EXPORTS ON)

# Turn on most warnings and treat all warnings as errors
target_link_libraries(lldbgui_lib PRIVATE lldbg_warnings)
target_link_libraries(lldbgui PRIVATE lldbg_warnings)

add_definitions(-DLLDBG_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets/")

target_include_directories(lldbgui_lib PUBLIC "${CMAKE_SOURCE_DIR}/src/")
target_include_directories(lldbgui PUBLIC "${CMAKE_SOURCE_DIR}/src/")

target_compile_definitions(lldbgui_lib PUBLIC IMGUI_DEFINE_MATH_OPERATORS
                                              GLFW_INCLUDE_NONE)

target_link_libraries(
  lldbgui_lib
  PUBLIC lldbg::lldb
         OpenGL::GL
         glfw
         fmt::fmt
         imgui::imgui
         ImGuiFileDialog::ImGuiFileDialog
         cxxopts::cxxopts)

option(LLDBG_BUILD_TESTS "Build unit tests" ON)
if(LLDBG_BUILD_TESTS)
  add_subdirectory(test)
endif()

