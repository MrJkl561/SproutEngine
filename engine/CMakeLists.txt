# The Sprout engine library. Games link to it with sprout_add_game().
add_library(sprout STATIC
    src/audio.cpp
    src/camera.cpp
    src/color.cpp
    src/entity.cpp
    src/game.cpp
    src/graphics.cpp
    src/input.cpp
    src/math.cpp
    src/particles.cpp
    src/physics.cpp
    src/random.cpp
    src/save.cpp
    src/scene.cpp
    src/sprite.cpp
    src/tilemap.cpp
    src/timer.cpp
    src/ui.cpp
)
add_library(sprout::sprout ALIAS sprout)

target_include_directories(sprout
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${raygui_SOURCE_DIR}/src  # raygui.h
            ${raygui_SOURCE_DIR}      # styles/<name>/style_<name>.h
)

target_compile_features(sprout PUBLIC cxx_std_20)

# raylib-cpp is PUBLIC so advanced users can #include <raylib-cpp.hpp>
# (see sprout/raylib_interop.hpp).
target_link_libraries(sprout
    PUBLIC raylib raylib_cpp nlohmann_json::nlohmann_json
)

# Extra warnings for the engine's own code (not for your games).
if(MSVC)
    target_compile_options(sprout PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(sprout PRIVATE -Wall -Wextra -Wpedantic)
endif()
