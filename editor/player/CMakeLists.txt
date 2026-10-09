# =============================================================================
#  An exported game: the game runtime (main.cpp), the editor's node code and
#  your C++ scripts, built for one platform. The editor's Export window
#  configures and builds this; you don't normally run it by hand.
#
#  Settings (passed by the editor):
#    SPROUT_GAME_NAME         the program's name
#    SPROUT_GAME_PROJECT_DIR  the project folder (scenes, assets, project.json)
#    SPROUT_GAME_SCRIPTS_DIR  the folder of C++ scripts
#  Libraries come from FETCHCONTENT_SOURCE_DIR_* (the editor's own copies),
#  so exporting doesn't download anything.
# =============================================================================
cmake_minimum_required(VERSION 3.25)
project(SproutGame LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(SPROUT_GAME_NAME "game" CACHE STRING "The game's program name")
set(SPROUT_GAME_PROJECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../project" CACHE PATH "The project folder")
set(SPROUT_GAME_SCRIPTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../project/scripts" CACHE PATH "The C++ scripts folder")

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/SproutDependencies.cmake")
if(NOT EMSCRIPTEN)
    include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/SproutEnet.cmake")  # multiplayer (not on the web)
endif()

set(EDITOR_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../src")
file(GLOB GAME_SCRIPTS CONFIGURE_DEPENDS "${SPROUT_GAME_SCRIPTS_DIR}/*.cpp")

add_executable(game
    main.cpp
    "${EDITOR_SRC}/game_runner.cpp"
    "${EDITOR_SRC}/console.cpp"
    "${EDITOR_SRC}/multiplayer.cpp"
    "${EDITOR_SRC}/net_transport.cpp"
    "${EDITOR_SRC}/assets.cpp"
    "${EDITOR_SRC}/nodes.cpp"
    "${EDITOR_SRC}/project.cpp"
    "${EDITOR_SRC}/scene_io.cpp"
    "${EDITOR_SRC}/scene_tree.cpp"
    "${EDITOR_SRC}/signals.cpp"
    "${EDITOR_SRC}/shaders.cpp"
    ${GAME_SCRIPTS})
target_include_directories(game PRIVATE "${EDITOR_SRC}")
target_link_libraries(game PRIVATE raylib raylib_cpp nlohmann_json::nlohmann_json)
if(TARGET sprout_enet)
    target_link_libraries(game PRIVATE sprout_enet)
endif()
set_target_properties(game PROPERTIES OUTPUT_NAME "${SPROUT_GAME_NAME}")

# Everything ends up in out/, for every build type.
set(_out "${CMAKE_BINARY_DIR}/out")
set_target_properties(game PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_out}")
foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" config)
    set_target_properties(game PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config} "${_out}")
endforeach()

if(EMSCRIPTEN)
    # A web page: game.html + .js + .wasm, and the project packed into .data.
    target_compile_definitions(game PRIVATE PLATFORM_WEB)
    target_compile_options(game PRIVATE -fexceptions)
    target_link_options(game PRIVATE -fexceptions -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576
        "--preload-file" "${SPROUT_GAME_PROJECT_DIR}@/project"
        "--shell-file" "${CMAKE_CURRENT_SOURCE_DIR}/shell.html")
    set_property(TARGET game APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shell.html")
    set_target_properties(game PROPERTIES SUFFIX ".html")
elseif(WIN32)
    # A normal Windows program: no console window behind the game.
    set_target_properties(game PROPERTIES WIN32_EXECUTABLE ON)
    if(MSVC)
        target_link_options(game PRIVATE /ENTRY:mainCRTStartup)
    else()
        # MinGW: put its runtime inside the .exe, so it runs on any Windows PC.
        target_link_options(game PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
elseif(APPLE)
    set_target_properties(game PROPERTIES
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_BUNDLE_NAME "${SPROUT_GAME_NAME}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.sprout.${SPROUT_GAME_NAME}")
endif()
