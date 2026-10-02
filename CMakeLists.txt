# =============================================================================
#  Sprout Engine - a friendly C++20 game engine
#
#  You normally don't need to touch this file!
#  To make a new game, create a folder inside "games/" with a main.cpp in it,
#  then build again. See docs/02-your-first-game.md.
# =============================================================================
cmake_minimum_required(VERSION 3.20)

project(Sprout
    VERSION 0.1.0
    DESCRIPTION "A beginner friendly C++ game engine built on raylib"
    LANGUAGES C CXX
)

# Is Sprout the main project, or was it pulled into someone else's project?
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(SPROUT_IS_TOP_LEVEL ON)
else()
    set(SPROUT_IS_TOP_LEVEL OFF)
endif()

option(SPROUT_BUILD_GAMES    "Build every game inside the games/ folder" ${SPROUT_IS_TOP_LEVEL})
option(SPROUT_BUILD_EXAMPLES "Build the example games in examples/"     ${SPROUT_IS_TOP_LEVEL})
option(SPROUT_BUILD_TESTS    "Build the engine's unit tests"            OFF)

# If nobody picked a build type, use Debug so the debugger works nicely.
if(SPROUT_IS_TOP_LEVEL AND NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
endif()

if(SPROUT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # helps code editors understand the project
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(SproutDependencies) # downloads raylib, raygui and nlohmann/json
add_subdirectory(engine)    # the engine itself (the "sprout" library)
include(SproutGame)         # gives you sprout_add_game(...)

if(SPROUT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(SPROUT_BUILD_GAMES)
    add_subdirectory(games)
endif()

if(SPROUT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
