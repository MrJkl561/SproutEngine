# =============================================================================
#  sprout_add_game(<name> [SOURCES file.cpp ...] [ASSETS folder])
#
#  Turns a folder of .cpp files into a game you can run.
#
#    sprout_add_game(my_game)                  # uses every .cpp in this folder
#    sprout_add_game(my_game SOURCES main.cpp player.cpp)
#    sprout_add_game(my_game ASSETS art)       # copy "art/" instead of "assets/"
#
#  The finished game is placed in   <build folder>/bin/<name>/
#  and the assets folder is copied next to it, so "assets/player.png" just works.
#
#  sprout_add_games_in(<folder>)
#
#  Makes a game out of every sub-folder of <folder>. A sub-folder that has its
#  own CMakeLists.txt is added with add_subdirectory() instead, so you can still
#  customise a game if you need to.
# =============================================================================

set(SPROUT_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin" CACHE PATH "Where finished games are placed")

function(sprout_add_game name)
    # FOLDER is used by sprout_add_games_in(); normally it's the current folder.
    cmake_parse_arguments(ARG "" "ASSETS;FOLDER" "SOURCES" ${ARGN})

    if(NOT ARG_FOLDER)
        set(ARG_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    if(NOT ARG_SOURCES)
        file(GLOB ARG_SOURCES CONFIGURE_DEPENDS
            "${ARG_FOLDER}/*.cpp"
            "${ARG_FOLDER}/*.hpp"
            "${ARG_FOLDER}/*.h")
    endif()
    if(NOT ARG_SOURCES)
        message(WARNING "Sprout: the game '${name}' has no .cpp files in ${ARG_FOLDER}, skipping it.")
        return()
    endif()

    if(NOT ARG_ASSETS)
        set(ARG_ASSETS assets)
    endif()

    set(out_dir "${SPROUT_OUTPUT_DIR}/${name}")

    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE sprout::sprout)
    target_compile_features(${name} PRIVATE cxx_std_20)

    # Put the game in bin/<name>/ for every build type (Debug, Release, ...)
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${out_dir}")
    foreach(config ${CMAKE_CONFIGURATION_TYPES} Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER "${config}" config)
        set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config} "${out_dir}")
    endforeach()

    # Visual Studio: run the game from its own folder when you press F5
    set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${out_dir}")

    # Copy the assets folder next to the game on every build, so changed
    # pictures and sounds are picked up even when no code changed.
    set(assets_dir "${ARG_FOLDER}/${ARG_ASSETS}")
    if(IS_DIRECTORY "${assets_dir}")
        add_custom_target(${name}_assets ALL
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${assets_dir}" "${out_dir}/assets"
            COMMENT "Copying assets for ${name}"
            VERBATIM)
        add_dependencies(${name} ${name}_assets)
    endif()

    # Make the first game the one Visual Studio starts by default
    get_property(startup DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT)
    if(NOT startup)
        set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT ${name})
    endif()
endfunction()

function(sprout_add_games_in folder)
    file(GLOB children RELATIVE "${folder}" CONFIGURE_DEPENDS "${folder}/*")
    list(SORT children)
    foreach(child ${children})
        set(path "${folder}/${child}")
        if(NOT IS_DIRECTORY "${path}")
            continue()
        endif()
        if(EXISTS "${path}/CMakeLists.txt")
            add_subdirectory("${path}")
            continue()
        endif()
        file(GLOB sources CONFIGURE_DEPENDS "${path}/*.cpp")
        if(NOT sources)
            continue()
        endif()
        # Folder names can contain spaces and other symbols; target names can't.
        string(REGEX REPLACE "[^A-Za-z0-9_.+-]" "_" target_name "${child}")
        sprout_add_game(${target_name} FOLDER "${path}")
    endforeach()
endfunction()
