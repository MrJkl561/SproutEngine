# =============================================================================
#  Sprout's unit tests.
#
#  These test the parts of the engine that don't need a window or a sound card
#  (math, colors, random numbers, timers, tile maps, physics, scenes and
#  entities, sprite animation, particles, save files and input actions), so
#  they can run anywhere - including on a CI server without a display.
#
#  Build and run them with:
#    cmake -S . -B build -DSPROUT_BUILD_TESTS=ON
#    cmake --build build
#    ctest --test-dir build --output-on-failure
# =============================================================================
include(FetchContent)

# --- doctest (a small, fast C++ testing framework) ----------------------------
set(SPROUT_DOCTEST_VERSION "v2.4.12" CACHE STRING "doctest git tag to download")

set(DOCTEST_WITH_TESTS               OFF CACHE BOOL "" FORCE)
set(DOCTEST_WITH_MAIN_IN_STATIC_LIB  OFF CACHE BOOL "" FORCE)
set(DOCTEST_NO_INSTALL               ON  CACHE BOOL "" FORCE)

FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        ${SPROUT_DOCTEST_VERSION}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

message(STATUS "Sprout: getting doctest ${SPROUT_DOCTEST_VERSION} for the unit tests...")
FetchContent_MakeAvailable(doctest)

# --- The test program -----------------------------------------------------------
add_executable(sprout_tests
    main.cpp
    test_math.cpp
    test_color.cpp
    test_random.cpp
    test_timer.cpp
    test_tilemap.cpp
    test_physics.cpp
    test_scene.cpp
    test_sprite.cpp
    test_particles.cpp
    test_save.cpp
    test_input.cpp
)

target_link_libraries(sprout_tests PRIVATE sprout::sprout doctest::doctest)
target_compile_features(sprout_tests PRIVATE cxx_std_20)

# The tests should be as warning-free as the engine itself.
if(MSVC)
    target_compile_options(sprout_tests PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(sprout_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME sprout_tests COMMAND sprout_tests)
