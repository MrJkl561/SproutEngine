#pragma once
// =============================================================================
//  sprout/raylib_interop.hpp - ADVANCED: use raylib-cpp together with Sprout
//
//  You don't need this to make games! It's for when you want something
//  Sprout doesn't have (gradients, shaders, render textures, 3D, ...).
//
//  Sprout is built on raylib-cpp, the C++ wrapper for raylib:
//      https://github.com/RobLoach/raylib-cpp
//  This header includes all of raylib-cpp and lets you convert between
//  Sprout's types and raylib-cpp's types:
//
//      #include <sprout/sprout.hpp>
//      #include <sprout/raylib_interop.hpp>
//
//      void draw() override {
//          sprout::Rect box{10, 10, 200, 50};
//          toRaylib(box).DrawGradientH(toRaylib(sprout::Color::Red), toRaylib(sprout::Color::Blue));
//      }
//
//  WATCH OUT: raylib has its own global types called Color, Rectangle,
//  Texture, Camera, etc. In a file that includes this header, write
//  sprout::Color or raylib::Color (not just Color) so the compiler knows
//  which one you mean.
// =============================================================================
#include <raylib-cpp.hpp>

#include "sprout/camera.hpp"
#include "sprout/color.hpp"
#include "sprout/graphics.hpp"
#include "sprout/math.hpp"

namespace sprout {

// --- Sprout -> raylib-cpp ------------------------------------------------------
inline raylib::Vector2 toRaylib(Vec2 v) { return raylib::Vector2{v.x, v.y}; }
inline raylib::Rectangle toRaylib(const Rect& r) { return raylib::Rectangle{r.x, r.y, r.width, r.height}; }
inline raylib::Color toRaylib(Color c) { return raylib::Color{c.r, c.g, c.b, c.a}; }
/// A view of a Sprout picture. Sprout still owns it, so don't Unload() it.
inline raylib::TextureUnmanaged toRaylib(const Texture& t) {
    return raylib::TextureUnmanaged{t.id, t.width, t.height, t.mipmaps, t.format};
}
/// The scene camera as raylib sees it (including any screen shake).
raylib::Camera2D toRaylib(const Camera& camera);

// --- raylib-cpp -> Sprout ------------------------------------------------------
inline Vec2 fromRaylib(const ::Vector2& v) { return Vec2{v.x, v.y}; }
inline Rect fromRaylib(const ::Rectangle& r) { return Rect{r.x, r.y, r.width, r.height}; }
inline Color fromRaylib(const ::Color& c) { return Color{c.r, c.g, c.b, c.a}; }
inline Texture fromRaylib(const ::Texture& t) { return Texture{t.id, t.width, t.height, t.mipmaps, t.format}; }

} // namespace sprout
