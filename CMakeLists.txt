cmake_minimum_required(VERSION 3.16...4.1)

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
target_include_directories(lldbgui_lib
                           PUBLIC "${CMAKE_SOURCE_DIR}/third_party/")
target_include_directories(lldbgui PUBLIC "${CMAKE_SOURCE_DIR}/src/")
target_include_directories(lldbgui PUBLIC "${CMAKE_SOURCE_DIR}/third_party/")

find_library(
  LLDB_LIBRARY
  NAMES lldb liblldb lldb-18
  PATHS "/Applications/Xcode.app/Contents/SharedFrameworks/"
        "/usr/lib/x86_64-linux-gnu/"
        "/Applications/Xcode15.0.1/Xcode.app/Contents/SharedFrameworks/")

target_compile_definitions(lldbgui_lib PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
target_link_libraries(
  lldbgui_lib
  PUBLIC ${LLDB_LIBRARY} # existing LLDB find_library result
         OpenGL::GL # from find_package(OpenGL)
         glfw # normalized target
         GLEW::GLEW # normalized/alias target
         fmt::fmt
         imgui::imgui
         ImGuiFileDialog::ImGuiFileDialog
         cxxopts::cxxopts)

if(NOT LLDB_LIBRARY)
  message(FATAL_ERROR "LLDB not found")
endif()
target_include_directories(lldbgui_lib
                           PUBLIC "third_party/llvm-18.1.0/lldb/include")

add_subdirectory(test)
