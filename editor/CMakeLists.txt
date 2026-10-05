# =============================================================================
#  Sprout Editor - a small Godot-style 2D editor (raylib + raylib-cpp + raygui)
#
#  Built together with the rest of Sprout (option SPROUT_BUILD_EDITOR), or on
#  its own:   cmake -S editor -B build-editor && cmake --build build-editor
#
#  Each project's C++ scripts (<project>/scripts/*.cpp) are built into a library
#  by scripts_module/, which the editor rebuilds and reloads by itself.
# =============================================================================
cmake_minimum_required(VERSION 3.25)

if(NOT TARGET raylib)
    # Standalone build: get raylib, raylib-cpp, raygui and nlohmann/json the
    # same way the main Sprout build does.
    project(SproutEditor LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    endif()
    include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/SproutDependencies.cmake")
    set(SPROUT_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin" CACHE PATH "Where programs are placed")
endif()

file(GLOB SPROUT_EDITOR_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp")
# Without hot reloading, one project's scripts are compiled into the editor
# (the demo project's, unless you point this somewhere else).
set(SPROUT_EDITOR_STATIC_SCRIPTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/project/scripts" CACHE PATH
    "Without hot reloading: the scripts folder compiled into the editor")
file(GLOB SPROUT_EDITOR_SCRIPTS CONFIGURE_DEPENDS
    "${SPROUT_EDITOR_STATIC_SCRIPTS_DIR}/*.cpp"
    "${SPROUT_EDITOR_STATIC_SCRIPTS_DIR}/*.hpp")

# Hot reloading: the scripts are built as a separate library that the editor
# rebuilds and reloads whenever a script file changes. The library uses the
# editor's own copy of the nodes and raylib, so the editor executable exports
# them (ENABLE_EXPORTS). On Windows that needs Visual Studio's compiler (MSVC or
# clang-cl); with MinGW the scripts are compiled into the editor instead
# (rebuild and restart).
if(WIN32 AND NOT MSVC)
    set(_hot_reload_default OFF)
else()
    set(_hot_reload_default ON)
endif()
option(SPROUT_EDITOR_HOT_RELOAD "Reload the editor's C++ scripts while it runs" ${_hot_reload_default})

if(SPROUT_EDITOR_HOT_RELOAD)
    add_executable(sprout_editor ${SPROUT_EDITOR_SOURCES})
else()
    add_executable(sprout_editor ${SPROUT_EDITOR_SOURCES} ${SPROUT_EDITOR_SCRIPTS})
endif()
target_compile_features(sprout_editor PRIVATE cxx_std_20)
target_include_directories(sprout_editor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${raygui_SOURCE_DIR}/src"   # raygui.h
    "${raygui_SOURCE_DIR}"       # styles/dark/style_dark.h
    "${raylib_SOURCE_DIR}/src/external/glfw/include")  # GLFW, to read the display scale
target_link_libraries(sprout_editor PRIVATE raylib_cpp nlohmann_json::nlohmann_json)

# The editor opens the project folder and writes new scripts straight into the
# source tree, so "New Script" just works.
target_compile_definitions(sprout_editor PRIVATE
    SPROUT_EDITOR_PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/project"
    SPROUT_EDITOR_STATIC_SCRIPTS_DIR="${SPROUT_EDITOR_STATIC_SCRIPTS_DIR}"
    # The dark theme's font (CC0), loaded at the right size for the UI scale.
    SPROUT_EDITOR_FONT="${raygui_SOURCE_DIR}/styles/dark/PixelOperator.ttf"
    # For rebuilding scripts and exporting games: CMake, the build folder, and the
    # libraries' sources (exports reuse them instead of downloading again).
    SPROUT_EDITOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    SPROUT_EDITOR_CMAKE="${CMAKE_COMMAND}"
    SPROUT_EDITOR_BUILD_DIR="${CMAKE_BINARY_DIR}"
    SPROUT_EDITOR_DEPS_DIR="${FETCHCONTENT_BASE_DIR}")

set(_editor_out "${SPROUT_OUTPUT_DIR}/sprout_editor")

if(SPROUT_EDITOR_HOT_RELOAD)
    # All of raylib goes into the editor (not just the parts the editor itself
    # uses), so scripts can call any raylib function.
    target_link_libraries(sprout_editor PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,raylib>" ${CMAKE_DL_LIBS})
    set_target_properties(sprout_editor PROPERTIES ENABLE_EXPORTS ON)

    if(WIN32)
        # An .exe exports nothing by default. Export the editor's own code
        # (WINDOWS_EXPORT_ALL_SYMBOLS) plus every raylib function, listed in a
        # .def file made from raylib.h; CMake merges the two.
        include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/RaylibExports.cmake")
        set(_raylib_h "${raylib_SOURCE_DIR}/src/raylib.h")
        set(_raylib_def "${CMAKE_CURRENT_BINARY_DIR}/raylib_exports.def")
        sprout_editor_raylib_def("${_raylib_h}" "${_raylib_def}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_raylib_h}")
        target_sources(sprout_editor PRIVATE "${_raylib_def}")
        set_target_properties(sprout_editor PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    endif()

    # What the editor needs to rebuild and load the scripts by itself.
    # Each project's scripts are built by scripts_module/ (the editor runs CMake
    # for it), with the same build type as the editor.
    target_compile_definitions(sprout_editor PRIVATE
        SPROUT_EDITOR_HOT_RELOAD=1
        SPROUT_EDITOR_BUILD_CONFIG="$<CONFIG>")
    if(WIN32)
        target_compile_definitions(sprout_editor PRIVATE SPROUT_EDITOR_IMPLIB="$<TARGET_LINKER_FILE:sprout_editor>")
    endif()
else()
    target_link_libraries(sprout_editor PRIVATE raylib)
endif()

# Windows: the native "Select Folder" window (COM).
if(WIN32)
    target_link_libraries(sprout_editor PRIVATE ole32 shell32 uuid)
endif()

if(MSVC)
    target_compile_options(sprout_editor PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(sprout_editor PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(sprout_editor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${_editor_out}"
    VS_DEBUGGER_WORKING_DIRECTORY "${_editor_out}")
foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" config)
    set_target_properties(sprout_editor PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config} "${_editor_out}")
endforeach()
