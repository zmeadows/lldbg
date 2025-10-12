# ---- macOS toolchain hygiene (keep local builds aligned with CI) ----
if(APPLE)
  # Prefer official CMake packages (e.g. LLDB/LLVM) over Find modules.
  set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)

  # Make the target arch explicit if the user didn't specify it.
  if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
      set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Target macOS arch"
                                                FORCE)
    else()
      set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Target macOS arch"
                                                 FORCE)
    endif()
  endif()

  # Enforce AppleClang by default to avoid libc++/SDK ABI mismatches.
  # Allow override via -DLLDBG_ALLOW_NON_APPLECLANG=ON for power users.
  option(LLDBG_ALLOW_NON_APPLECLANG "Allow non-Apple Clang on macOS" OFF)

  execute_process(
    COMMAND "${CMAKE_CXX_COMPILER}" --version OUTPUT_VARIABLE _LLDBG_CXX_VER
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)

  if(NOT LLDBG_ALLOW_NON_APPLECLANG)
    if(NOT
       _LLDBG_CXX_VER
       MATCHES
       "Apple[ ]+clang")
      message(
        FATAL_ERROR
          "On macOS, build with AppleClang to avoid ABI issues.\n"
          "Tip: export CC=\"$(xcrun --find clang)\" CXX=\"$(xcrun --find clang++)\" "
          "or pass -DLLDBG_ALLOW_NON_APPLECLANG=ON if you know what you're doing.\n"
          "Detected compiler: ${CMAKE_CXX_COMPILER} — ${_LLDBG_CXX_VER}")
    endif()
  endif()

  message(
    STATUS "macOS: C++ compiler = ${CMAKE_CXX_COMPILER} — ${_LLDBG_CXX_VER}")
  message(STATUS "macOS: CMAKE_OSX_ARCHITECTURES = ${CMAKE_OSX_ARCHITECTURES}")
endif()
