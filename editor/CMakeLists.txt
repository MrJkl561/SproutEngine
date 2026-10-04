# =============================================================================
#  Sprout Editor - a small Godot-style 2D editor (raylib + raylib-cpp + raygui)
#
#  Built together with the rest of Sprout (option SPROUT_BUILD_EDITOR), or on
#  its own:   cmake -S editor -B build-editor && cmake --build build-editor
#
#  Every .cpp file in scripts/ is built into the sprout_scripts library, which
#  the editor rebuilds and reloads by itself when a script changes.
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
file(GLOB SPROUT_EDITOR_SCRIPTS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/*.hpp")

# Hot reloading: the scripts are built as a separate library that the editor
# rebuilds and reloads whenever a script file changes. The library uses the
# editor's own copy of the nodes and raylib, which needs an executable that
# exports its symbols; Windows can't do that with a static raylib, so there the
# scripts are compiled into the editor instead (rebuild and restart).
if(WIN32)
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
    SPROUT_EDITOR_SCRIPTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/scripts"
    # The dark theme's font (CC0), loaded at the right size for the UI scale.
    SPROUT_EDITOR_FONT="${raygui_SOURCE_DIR}/styles/dark/PixelOperator.ttf")

set(_editor_out "${SPROUT_OUTPUT_DIR}/sprout_editor")

if(SPROUT_EDITOR_HOT_RELOAD)
    # All of raylib goes into the editor (not just the parts the editor itself
    # uses), so scripts can call any raylib function.
    target_link_libraries(sprout_editor PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,raylib>" ${CMAKE_DL_LIBS})
    set_target_properties(sprout_editor PROPERTIES ENABLE_EXPORTS ON)

    # src/module/ holds an empty file so the library builds even with no scripts.
    add_library(sprout_scripts MODULE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/module/scripts_module.cpp"
        ${SPROUT_EDITOR_SCRIPTS})
    target_compile_features(sprout_scripts PRIVATE cxx_std_20)
    target_include_directories(sprout_scripts PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "$<TARGET_PROPERTY:raylib,INTERFACE_INCLUDE_DIRECTORIES>"
        "$<TARGET_PROPERTY:raylib_cpp,INTERFACE_INCLUDE_DIRECTORIES>")
    # Link against the editor: node types, print(), raylib all come from it.
    target_link_libraries(sprout_scripts PRIVATE sprout_editor)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # Lets an old copy of the library really unload when a new one is loaded.
        target_compile_options(sprout_scripts PRIVATE -fno-gnu-unique)
    endif()
    if(NOT MSVC)
        target_compile_options(sprout_scripts PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    set_target_properties(sprout_scripts PROPERTIES
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY "${_editor_out}")
    foreach(config Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER "${config}" config)
        set_target_properties(sprout_scripts PROPERTIES LIBRARY_OUTPUT_DIRECTORY_${config} "${_editor_out}")
    endforeach()

    # What the editor needs to rebuild and load the scripts by itself.
    target_compile_definitions(sprout_editor PRIVATE
        SPROUT_EDITOR_HOT_RELOAD=1
        SPROUT_EDITOR_SCRIPTS_MODULE="$<TARGET_FILE:sprout_scripts>"
        SPROUT_EDITOR_CMAKE="${CMAKE_COMMAND}"
        SPROUT_EDITOR_BUILD_DIR="${CMAKE_BINARY_DIR}"
        SPROUT_EDITOR_BUILD_CONFIG="$<CONFIG>")
else()
    target_link_libraries(sprout_editor PRIVATE raylib)
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
