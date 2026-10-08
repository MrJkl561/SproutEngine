# Builds Windows games from Linux or macOS with the MinGW-w64 cross compiler
# (Ubuntu/Debian: sudo apt install mingw-w64; macOS: brew install mingw-w64).
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(_prefix x86_64-w64-mingw32)
set(CMAKE_C_COMPILER ${_prefix}-gcc)
set(CMAKE_CXX_COMPILER ${_prefix}-g++)
set(CMAKE_RC_COMPILER ${_prefix}-windres)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
