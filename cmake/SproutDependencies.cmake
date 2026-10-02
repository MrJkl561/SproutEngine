# =============================================================================
#  Downloads the open source libraries Sprout is built on.
#  This happens automatically the first time you run CMake (it needs git and
#  an internet connection). After that everything is cached in your build
#  folder, so later builds are fast and work offline.
#
#    raylib        - window, drawing, keyboard/mouse/gamepad, sound   (zlib license)
#    raygui        - buttons, sliders and other in-game menus          (zlib license)
#    nlohmann/json - reading and writing JSON (save files, level data) (MIT license)
# =============================================================================
include(FetchContent)

set(SPROUT_RAYLIB_VERSION "6.0"     CACHE STRING "raylib git tag to download")
set(SPROUT_RAYGUI_VERSION "5.0"     CACHE STRING "raygui git tag to download")
set(SPROUT_JSON_VERSION   "v3.12.0" CACHE STRING "nlohmann/json git tag to download")

# --- raylib -----------------------------------------------------------------
# We only want the library, not raylib's own examples and games.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)

FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        ${SPROUT_RAYLIB_VERSION}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

# --- raygui (a single header file, no CMakeLists of its own) ----------------
FetchContent_Declare(raygui
    GIT_REPOSITORY https://github.com/raysan5/raygui.git
    GIT_TAG        ${SPROUT_RAYGUI_VERSION}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

# --- nlohmann/json ------------------------------------------------------------
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install    OFF CACHE INTERNAL "")

FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        ${SPROUT_JSON_VERSION}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

message(STATUS "Sprout: getting raylib ${SPROUT_RAYLIB_VERSION}, raygui ${SPROUT_RAYGUI_VERSION} "
               "and nlohmann/json ${SPROUT_JSON_VERSION} (first time can take a minute)...")
FetchContent_MakeAvailable(raylib raygui nlohmann_json)
