cmake_minimum_required(VERSION 3.11)

project(lldbgui VERSION 0.1
                DESCRIPTION "A GUI for lldb."
                LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEBUG")
  set(ENV{ASAN_OPTIONS} "detect_container_overflow=0")
  set(
    CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -Werror -fsanitize=undefined -fsanitize=address"
  )
endif()

file(GLOB LLDBGUI_SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*/*.cpp"
    "${CMAKE_SOURCE_DIR}/third_party/imgui-1.90.9/*.cpp"
)

# Remove main.cpp from the sources for the library
list(REMOVE_ITEM LLDBGUI_SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp")

add_definitions(-DLLDBG_TESTS_DIR="${CMAKE_SOURCE_DIR}/test/")

# Create a library for lldbgui
add_library(lldbgui_lib ${LLDBGUI_SOURCES})

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  target_link_options(lldbgui_lib
    BEFORE PUBLIC -fsanitize=undefined PUBLIC -fsanitize=address
  )
endif()

# Create the lldbgui executable with main.cpp
add_executable(lldbgui ${CMAKE_SOURCE_DIR}/src/main.cpp)
target_link_libraries(lldbgui lldbgui_lib)

# Enable exports for the library
set_target_properties(lldbgui_lib PROPERTIES ENABLE_EXPORTS ON)

# Turn on most warnings and treat all warnings as errors
target_compile_options(lldbgui_lib PUBLIC -Wall -Wextra -pedantic -Werror)
target_compile_options(lldbgui PUBLIC -Wall -Wextra -pedantic -Werror)

add_definitions(-DLLDBG_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets/")

target_include_directories(lldbgui_lib PUBLIC "${CMAKE_SOURCE_DIR}/src/")
target_include_directories(lldbgui_lib PUBLIC "${CMAKE_SOURCE_DIR}/third_party/")
target_include_directories(lldbgui PUBLIC "${CMAKE_SOURCE_DIR}/src/")
target_include_directories(lldbgui PUBLIC "${CMAKE_SOURCE_DIR}/third_party/")

find_library(LLDB_LIBRARY NAMES lldb liblldb lldb-18 PATHS "/Applications/Xcode.app/Contents/SharedFrameworks/" "/usr/lib/x86_64-linux-gnu/" "/Applications/Xcode15.0.1/Xcode.app/Contents/SharedFrameworks/")
target_link_libraries(lldbgui_lib ${LLDB_LIBRARY})
if (NOT LLDB_LIBRARY)
    message(FATAL_ERROR "LLDB not found")
endif()
target_include_directories(lldbgui_lib PUBLIC "third_party/llvm-18.1.0/lldb/include")

set(OpenGL_GL_PREFERENCE "GLVND")
find_package(OpenGL REQUIRED)

target_include_directories(lldbgui_lib PUBLIC "third_party/glew-2.1.0/include")
add_definitions(-DGLEW_STATIC)
add_subdirectory(./third_party/glew-2.1.0)
target_link_libraries(lldbgui_lib libglew_static)

target_include_directories(lldbgui_lib PUBLIC "third_party/fmt-11.0.1/include")
add_subdirectory(./third_party/fmt-11.0.1/)
target_link_libraries(lldbgui_lib fmt)

target_include_directories(lldbgui_lib PUBLIC "third_party/glfw-3.4/include")
set(ENKITS_BUILD_EXAMPLES OFF CACHE BOOL "Build basic example applications")
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "GLFW lib only")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "GLFW lib only")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "GLFW lib only")
set(GLFW_BUILD_INSTALL OFF CACHE BOOL "GLFW lib only")
add_subdirectory(./third_party/glfw-3.4)
target_link_libraries(lldbgui_lib glfw ${OPENGL_glu_LIBRARY} ${GLFW_LIBRARIES})

target_include_directories(lldbgui_lib PUBLIC "third_party/cxxopts-2.2.0/include")

target_include_directories(lldbgui_lib PUBLIC "third_party/GSL-3.1.0/include/")

add_definitions(-DIMGUI_DEFINE_MATH_OPERATORS)
target_include_directories(lldbgui_lib PUBLIC "third_party/imgui-1.90.9/")

target_include_directories(lldbgui_lib PUBLIC "third_party/ImGuiFileDialog/")
add_subdirectory(./third_party/ImGuiFileDialog/)
target_link_libraries(lldbgui_lib ImGuiFileDialog)

add_subdirectory(test)
