cmake_minimum_required(VERSION 3.16)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(cmake/PreferClang.cmake)

project(
  lldbgui
  VERSION 0.1
  DESCRIPTION "A GUI for lldb."
  LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/Warnings.cmake)
include(cmake/LLDBGDeps.cmake)

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

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

# if(CMAKE_BUILD_TYPE MATCHES "Debug")
#   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEBUG")
#   set(ENV{ASAN_OPTIONS} "detect_container_overflow=0")
#   set(CMAKE_CXX_FLAGS
#       "${CMAKE_CXX_FLAGS} -Werror -fsanitize=undefined -fsanitize=address")
# endif()

file(GLOB LLDBGUI_SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp"
     "${CMAKE_SOURCE_DIR}/src/*/*.cpp")

# Remove main.cpp from the sources for the library
list(REMOVE_ITEM LLDBGUI_SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp")

add_definitions(-DLLDBG_TESTS_DIR="${CMAKE_SOURCE_DIR}/test/")

# Create a library for lldbgui
add_library(lldbgui_lib ${LLDBGUI_SOURCES})

# if(CMAKE_BUILD_TYPE MATCHES "Debug")
#   target_link_options(lldbgui_lib BEFORE PUBLIC -fsanitize=undefined PUBLIC -fsanitize=address)
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

target_compile_definitions(lldbgui_lib PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
target_link_libraries(
  lldbgui_lib
  PUBLIC lldbg::lldb
         OpenGL::GL # from find_package(OpenGL)
         glfw # normalized target
         GLEW::GLEW # normalized/alias target
         fmt::fmt
         imgui::imgui
         ImGuiFileDialog::ImGuiFileDialog
         cxxopts::cxxopts)

add_subdirectory(test)
