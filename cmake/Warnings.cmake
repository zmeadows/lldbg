option(LLDBG_WARN_AS_ERRORS "Treat warnings as errors for project code" ON)

add_library(lldbg_warnings INTERFACE)
add_library(lldbg::warnings ALIAS lldbg_warnings)

# cmake-format: off
target_compile_options(lldbg_warnings INTERFACE
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:
    -Wall -Wextra -Wpedantic -Wformat=2
    #$<$<BOOL:${LLDBG_WARN_AS_ERRORS}>:-Werror>
  >
  $<$<CXX_COMPILER_ID:MSVC>:
    /W4
    #$<$<BOOL:${LLDBG_WARN_AS_ERRORS}>:/WX>
  >
)
# cmake-format: on
