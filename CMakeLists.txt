cmake_minimum_required(VERSION 3.0)
project(lldbgui)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

add_definitions(-DIMGUI_DEFINE_MATH_OPERATORS)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Werror -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -Werror -Wpedantic")

include_directories(src)
file(GLOB LLDBGUI_SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp" "${CMAKE_SOURCE_DIR}/src/*/*.cpp")

find_library(LLDB lldb)

find_package (Threads)

find_package(GLUT REQUIRED)
include_directories(${GLUT_INCLUDE_DIR})
if(NOT GLUT_FOUND)
    message(ERROR "GLUT not found!")
endif(NOT GLUT_FOUND)

find_package(OpenGL REQUIRED)
include_directories(${OpenGL_INCLUDE_DIRS})
link_directories(${OpenGL_LIBRARY_DIRS})
add_definitions(${OpenGL_DEFINITIONS})
if(NOT OPENGL_FOUND)
    message(ERROR "OPENGL not found!")
endif(NOT OPENGL_FOUND)

include_directories(lib/)
include_directories(lib/imgui)
include_directories(lib/imgui/examples)
file(GLOB IMGUI_SOURCES "${CMAKE_SOURCE_DIR}/lib/imgui/*.cpp" "${CMAKE_SOURCE_DIR}/lib/imgui/examples/*freeglut.cpp" "${CMAKE_SOURCE_DIR}/lib/imgui/examples/*opengl2.cpp")

include_directories(lib/ImGuiColorTextEdit)
file(GLOB IMGUI_COLOR_TEXT_EDIT_SOURCES "${CMAKE_SOURCE_DIR}/lib/ImGuiColorTextEdit/*.cpp")

add_executable(lldbgui ${CMAKE_SOURCE_DIR}/src/main.cpp ${LLDBGUI_SOURCES} ${IMGUI_SOURCES} ${IMGUI_COLOR_TEXT_EDIT_SOURCES})
target_link_libraries(lldbgui ${LLDB} ${GLUT_LIBRARIES} ${OPENGL_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(lldbgui stdc++fs)
