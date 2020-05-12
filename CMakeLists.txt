cmake_minimum_required(VERSION 3.0)

project(lldbgui VERSION 0.1
                DESCRIPTION "A GUI for lldb."
                LANGUAGES CXX C)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

file(GLOB LLDBGUI_SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*/*.cpp"
    "${CMAKE_SOURCE_DIR}/third_party/imgui-1.75/*.cpp"
    )

add_definitions(-DLLDBG_TESTS_DIR="${CMAKE_SOURCE_DIR}/test/")

add_executable(lldbgui ${CMAKE_SOURCE_DIR}/src/main.cpp ${LLDBGUI_SOURCES})

# turn on most warnings and treat all warnings as errors
if (MSVC)
    target_compile_options(lldbgui PUBLIC /W4 /WX)
else()
    target_compile_options(lldbgui PUBLIC -Wall -Wextra -pedantic -Werror)
endif()

add_definitions(-DLLDBG_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets/")

target_include_directories(lldbgui PUBLIC "${CMAKE_SOURCE_DIR}/src/")
target_include_directories(lldbgui PUBLIC "${CMAKE_SOURCE_DIR}/third_party/")

find_library(LLDB_LIBRARY lldb PATHS "/Applications/Xcode.app/Contents/SharedFrameworks/")
target_link_libraries(lldbgui ${LLDB_LIBRARY})
if (NOT LLDB_LIBRARY)
    message(FATAL_ERROR "LLDB not found")
endif()
target_include_directories(lldbgui PUBLIC "third_party/llvm-10.0.0/lldb/include")

set(OpenGL_GL_PREFERENCE "LEGACY")
find_package(OpenGL REQUIRED)

target_include_directories(lldbgui PUBLIC "third_party/glew-2.1.0/include")
add_definitions(-DGLEW_STATIC)
add_subdirectory(./third_party/glew-2.1.0)
target_link_libraries(lldbgui libglew_static)

target_include_directories(lldbgui PUBLIC "third_party/fmt-6.2.1/include")
add_subdirectory(./third_party/fmt-6.2.1/)
target_link_libraries(lldbgui fmt)

target_include_directories(lldbgui PUBLIC "third_party/glfw-3.3.2/include")
set( ENKITS_BUILD_EXAMPLES OFF CACHE BOOL "Build basic example applications" )
set( GLFW_BUILD_EXAMPLES OFF CACHE BOOL  "GLFW lib only" )
set( GLFW_BUILD_TESTS OFF CACHE BOOL  "GLFW lib only" )
set( GLFW_BUILD_DOCS OFF CACHE BOOL  "GLFW lib only" )
set( GLFW_BUILD_INSTALL OFF CACHE BOOL  "GLFW lib only" )
add_subdirectory(./third_party/glfw-3.3.2)
target_link_libraries(lldbgui glfw ${OPENGL_glu_LIBRARY} ${GLFW_LIBRARIES})

target_include_directories(lldbgui PUBLIC "third_party/cxxopts-2.2.0/include")

add_definitions(-DIMGUI_DEFINE_MATH_OPERATORS)
target_include_directories(lldbgui PUBLIC "third_party/imgui-1.75/")

