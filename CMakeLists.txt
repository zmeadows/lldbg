cmake_minimum_required(VERSION 3.20...4.1)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(cmake/PreferClang.cmake)

project(
  lldbgui
  VERSION 0.1
  DESCRIPTION "A GUI for lldb."
  LANGUAGES CXX C)

include(cmake/ApplePrelude.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

# Turn this on per-build (e.g., -DCMAKE_EXPOSE_COMPILE_COMMANDS=ON).
option(CMAKE_EXPOSE_COMPILE_COMMANDS
       "Expose compile_commands.json at repo root" OFF)

if(CMAKE_EXPOSE_COMPILE_COMMANDS)
  # Ensure the file is generated in the build dir.
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)

  # Create/update a link (or copy on platforms that can't symlink).
  add_custom_target(
    expose-compile-commands ALL
    COMMAND
      ${CMAKE_COMMAND} -Dsrc=${CMAKE_BINARY_DIR}/compile_commands.json
      -Ddst=${CMAKE_SOURCE_DIR}/compile_commands.json -P
      ${CMAKE_SOURCE_DIR}/cmake/ExposeCompileCommands.cmake
    BYPRODUCTS ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT
      "Exposing compile_commands.json at project root (symlink if possible)"
    VERBATIM)
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
