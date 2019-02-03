cmake_minimum_required(VERSION 3.0)
project(lldbgui)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

include_directories(src)
file(GLOB LLDBGUI_SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp" "${CMAKE_SOURCE_DIR}/src/*/*.cpp")

find_library(LLDB lldb)

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

include_directories(lib/imgui)
include_directories(lib/imgui/examples)
file(GLOB IMGUI_SOURCES "${CMAKE_SOURCE_DIR}/lib/imgui/*.cpp" "${CMAKE_SOURCE_DIR}/lib/imgui/examples/*freeglut.cpp" "${CMAKE_SOURCE_DIR}/lib/imgui/examples/*opengl2.cpp")

add_executable(lldbgui ${CMAKE_SOURCE_DIR}/src/main.cpp ${LLDBGUI_SOURCES} ${IMGUI_SOURCES})
target_link_libraries(lldbgui ${LLDB} ${GLUT_LIBRARIES} ${OPENGL_LIBRARIES})

