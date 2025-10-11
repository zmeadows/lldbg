# ===== File: cmake/PreferClang.cmake =========================================
# Prefer Clang automatically iff the user didn't preselect a compiler. Must be
# included BEFORE `project()` (and before any enable_language()).

option(LLDBG_PREFER_CLANG "Prefer Clang if available when not specified" ON)

if(LLDBG_PREFER_CLANG)
  # Respect any preselection (cache or environment) to avoid toolchain flips.
  set(_lldbg_preselected FALSE)
  if(CMAKE_C_COMPILER
     OR CMAKE_CXX_COMPILER
     OR DEFINED ENV{CC}
     OR DEFINED ENV{CXX})
    set(_lldbg_preselected TRUE)
  endif()

  if(NOT _lldbg_preselected)
    if(WIN32)
      # Windows: prefer clang-cl (MSVC-compatible).
      find_program(LLDBG_CLANG_CL NAMES clang-cl)
      if(LLDBG_CLANG_CL)
        set(CMAKE_C_COMPILER
            "${LLDBG_CLANG_CL}"
            CACHE FILEPATH "Preferred C compiler")
        set(CMAKE_CXX_COMPILER
            "${LLDBG_CLANG_CL}"
            CACHE FILEPATH "Preferred C++ compiler")
        set(LLDBG_COMPILER_SELECTED "clang-cl")
      endif()
    else()
      # POSIX: prefer clang/clang++
      find_program(LLDBG_CLANG NAMES clang)
      find_program(LLDBG_CLANGXX NAMES clang++)
      if(LLDBG_CLANG AND LLDBG_CLANGXX)
        set(CMAKE_C_COMPILER
            "${LLDBG_CLANG}"
            CACHE FILEPATH "Preferred C compiler")
        set(CMAKE_CXX_COMPILER
            "${LLDBG_CLANGXX}"
            CACHE FILEPATH "Preferred C++ compiler")
        set(LLDBG_COMPILER_SELECTED "clang/clang++")
      endif()
    endif()

    if(LLDBG_COMPILER_SELECTED)
      message(STATUS "LLDBG: defaulting to ${LLDBG_COMPILER_SELECTED} "
                     "(override with CC/CXX or -DCMAKE_C{,XX}_COMPILER).")
    endif()
  endif()
endif()

# ===== (Optional) File: cmake/toolchains/prefer-clang.cmake ==================
# Use this *instead* of the module if you prefer a toolchain file. Invoke with:
# cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/prefer-clang.cmake
# Or wire it into a CMakePresets.json later.

# Only set compilers if not already chosen.
if(NOT CMAKE_C_COMPILER AND NOT CMAKE_CXX_COMPILER)
  if(WIN32)
    find_program(_CLANG_CL NAMES clang-cl)
    if(_CLANG_CL)
      set(CMAKE_C_COMPILER
          "${_CLANG_CL}"
          CACHE FILEPATH "")
      set(CMAKE_CXX_COMPILER
          "${_CLANG_CL}"
          CACHE FILEPATH "")
      message(STATUS "Toolchain: using clang-cl")
    endif()
  else()
    find_program(_CLANG NAMES clang)
    find_program(_CLANGXX NAMES clang++)
    if(_CLANG AND _CLANGXX)
      set(CMAKE_C_COMPILER
          "${_CLANG}"
          CACHE FILEPATH "")
      set(CMAKE_CXX_COMPILER
          "${_CLANGXX}"
          CACHE FILEPATH "")
      message(STATUS "Toolchain: using clang/clang++")
    endif()
  endif()
endif()
