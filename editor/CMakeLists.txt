# =============================================================================
#  Sprout Editor - a small Godot-style 2D editor (raylib + raylib-cpp + raygui)
#
#  Built together with the rest of Sprout (option SPROUT_BUILD_EDITOR), or on
#  its own:   cmake -S editor -B build-editor && cmake --build build-editor
#
#  Every .cpp file in scripts/ is compiled into the editor, so a new script
#  shows up in the inspector after the next build.
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
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/*.cpp")

add_executable(sprout_editor ${SPROUT_EDITOR_SOURCES} ${SPROUT_EDITOR_SCRIPTS})
target_compile_features(sprout_editor PRIVATE cxx_std_20)
target_include_directories(sprout_editor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${raygui_SOURCE_DIR}/src"   # raygui.h
    "${raygui_SOURCE_DIR}")      # styles/dark/style_dark.h
target_link_libraries(sprout_editor PRIVATE raylib raylib_cpp nlohmann_json::nlohmann_json)

# The editor opens the project folder and writes new scripts straight into the
# source tree, so "New Script" + rebuild just works.
target_compile_definitions(sprout_editor PRIVATE
    SPROUT_EDITOR_PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/project"
    SPROUT_EDITOR_SCRIPTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/scripts")

if(MSVC)
    target_compile_options(sprout_editor PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(sprout_editor PRIVATE -Wall -Wextra -Wpedantic)
endif()

set(_editor_out "${SPROUT_OUTPUT_DIR}/sprout_editor")
set_target_properties(sprout_editor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${_editor_out}"
    VS_DEBUGGER_WORKING_DIRECTORY "${_editor_out}")
foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" config)
    set_target_properties(sprout_editor PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config} "${_editor_out}")
endforeach()
