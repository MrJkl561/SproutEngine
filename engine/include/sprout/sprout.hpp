#pragma once
// =============================================================================
//   ____                         _
//  / ___| _ __  _ __ ___  _   _| |_
//  \___ \| '_ \| '__/ _ \| | | | __|
//   ___) | |_) | | | (_) | |_| | |_
//  |____/| .__/|_|  \___/ \__,_|\__|   a friendly C++ game engine
//        |_|
//
//  Include this one file to get everything:
//
//      #include <sprout/sprout.hpp>
//      using namespace sprout;
//
//  Each part has its own header with explanations and examples at the top:
//
//    game.hpp       the window, running the game, switching scenes, time
//    scene.hpp      scenes (screens/levels): start, update, draw, timers
//    entity.hpp     the things in your game: players, enemies, bullets...
//    input.hpp      keyboard, mouse, gamepad and "actions"
//    graphics.hpp   drawing shapes, text and pictures
//    sprite.hpp     pictures that move, flip and animate
//    camera.hpp     following the player, zoom, screen shake
//    tilemap.hpp    levels made from letters
//    physics.hpp    moving without going through walls
//    particles.hpp  explosions and sparkles
//    audio.hpp      sound effects and music
//    timer.hpp      Timer and Cooldown
//    ui.hpp         buttons, sliders, checkboxes, text boxes
//    save.hpp       save files and JSON
//    random.hpp     random numbers
//    math.hpp       Vec2, Rect and math helpers
//    color.hpp      colors
// =============================================================================

#include "sprout/audio.hpp"
#include "sprout/camera.hpp"
#include "sprout/color.hpp"
#include "sprout/entity.hpp"
#include "sprout/game.hpp"
#include "sprout/graphics.hpp"
#include "sprout/input.hpp"
#include "sprout/math.hpp"
#include "sprout/particles.hpp"
#include "sprout/physics.hpp"
#include "sprout/random.hpp"
#include "sprout/save.hpp"
#include "sprout/scene.hpp"
#include "sprout/sprite.hpp"
#include "sprout/tilemap.hpp"
#include "sprout/timer.hpp"
#include "sprout/ui.hpp"

#define SPROUT_VERSION "0.1.0"
