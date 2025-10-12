# Usage: cmake -Dsrc=... -Ddst=... -P ExposeCompileCommands.cmake
if(NOT EXISTS "${src}")
  message(WARNING "Expected ${src}; is CMAKE_EXPORT_COMPILE_COMMANDS=ON?")
  return()
endif()

# Remove any old file/link and then try to symlink; fall back to copy if needed.
file(REMOVE "${dst}")
# Requires CMake ≥ 3.14. SYMBOLIC makes a symlink; COPY_ON_ERROR falls back to a real copy.
file(
  CREATE_LINK
  "${src}"
  "${dst}"
  SYMBOLIC
  COPY_ON_ERROR)
