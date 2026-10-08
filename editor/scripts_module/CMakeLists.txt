# =============================================================================
#  A project's C++ scripts, built as a library the editor loads while it runs
#  (hot reloading). The editor configures and builds this by itself for the
#  open project; you don't need to run it by hand.
#
#  Settings (passed by the editor):
#    SPROUT_SCRIPTS_DIR    the project's scripts/ folder
#    SPROUT_EDITOR_SRC     editor/src (script.hpp, nodes.hpp, ...)
#    SPROUT_DEPS_DIR       the editor's library sources (raylib-src, raylib_cpp-src)
#    SPROUT_EDITOR_IMPLIB  Windows: sprout_editor.lib, to link to the running editor
# =============================================================================
cmake_minimum_required(VERSION 3.25)
project(SproutScripts LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SPROUT_SCRIPTS_DIR "" CACHE PATH "The project's scripts folder")
set(SPROUT_EDITOR_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../src" CACHE PATH "editor/src")
set(SPROUT_DEPS_DIR "" CACHE PATH "Where raylib-src and raylib_cpp-src are")
set(SPROUT_EDITOR_IMPLIB "" CACHE FILEPATH "Windows: the editor's import library")

file(GLOB scripts CONFIGURE_DEPENDS "${SPROUT_SCRIPTS_DIR}/*.cpp")

# empty.cpp: so the library builds even when the project has no scripts yet.
add_library(sprout_scripts MODULE "${CMAKE_CURRENT_SOURCE_DIR}/empty.cpp" ${scripts})
target_include_directories(sprout_scripts PRIVATE
    "${SPROUT_EDITOR_SRC}"
    "${SPROUT_DEPS_DIR}/raylib-src/src"
    "${SPROUT_DEPS_DIR}/raylib_cpp-src/include")
set_target_properties(sprout_scripts PROPERTIES PREFIX "")

# The node types, print() and raylib all live in the running editor.
if(WIN32)
    target_link_libraries(sprout_scripts PRIVATE "${SPROUT_EDITOR_IMPLIB}")
elseif(APPLE)
    target_link_options(sprout_scripts PRIVATE -undefined dynamic_lookup)
endif()  # Linux: found when the editor loads the library

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # Lets an old copy of the library really unload when a new one is loaded.
    target_compile_options(sprout_scripts PRIVATE -fno-gnu-unique)
endif()
if(MSVC)
    target_compile_options(sprout_scripts PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(sprout_scripts PRIVATE -Wall -Wextra -Wpedantic)
endif()

# The library always ends up in out/.
set(_out "${CMAKE_BINARY_DIR}/out")
set_target_properties(sprout_scripts PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${_out}" RUNTIME_OUTPUT_DIRECTORY "${_out}")
foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" config)
    set_target_properties(sprout_scripts PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${config} "${_out}" RUNTIME_OUTPUT_DIRECTORY_${config} "${_out}")
endforeach()
