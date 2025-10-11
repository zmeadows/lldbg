option(LLDBG_WARN_AS_ERRORS "Treat warnings as errors for project code" ON)

add_library(lldbg_warnings INTERFACE)
add_library(lldbg::warnings ALIAS lldbg_warnings)

# cmake-format: off
target_compile_options(lldbg_warnings INTERFACE
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:
    -Wall -Wextra -Wpedantic -Wformat=2 -Wno-deprecated-literal-operator
  >
  $<$<CXX_COMPILER_ID:MSVC>:
    /W4
  >
)
# cmake-format: on

if(LLDBG_WARN_AS_ERRORS)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
    set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
  else()
    add_compile_options($<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Werror>
                        $<$<CXX_COMPILER_ID:MSVC>:/WX>)
  endif()
endif()
