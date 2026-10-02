// Tests for sprout/raylib_interop.hpp: converting between Sprout's types and
// raylib-cpp's types. None of these need a window.
//
// Note: raylib has its own global Color, Rectangle, Texture, Camera... so this
// file always writes sprout::Color / raylib::Color instead of plain Color.
#include <doctest/doctest.h>

#include <type_traits>
#include <vector>

#include <sprout/game.hpp>
#include <sprout/random.hpp>
#include <sprout/raylib_interop.hpp>

#include "test_helpers.hpp"

using doctest::Approx;
using sprout::fromRaylib;
using sprout::toRaylib;

// The conversions give raylib-cpp types (with all their methods), and take plain raylib structs.
static_assert(std::is_same_v<decltype(toRaylib(sprout::Vec2{})), raylib::Vector2>);
static_assert(std::is_same_v<decltype(toRaylib(sprout::Rect{})), raylib::Rectangle>);
static_assert(std::is_same_v<decltype(toRaylib(sprout::Color{})), raylib::Color>);
static_assert(std::is_same_v<decltype(toRaylib(sprout::Texture{})), raylib::TextureUnmanaged>);
static_assert(std::is_same_v<decltype(toRaylib(sprout::Camera{})), raylib::Camera2D>);
static_assert(std::is_same_v<decltype(fromRaylib(::Vector2{})), sprout::Vec2>);
static_assert(std::is_same_v<decltype(fromRaylib(::Rectangle{})), sprout::Rect>);
static_assert(std::is_same_v<decltype(fromRaylib(::Color{})), sprout::Color>);
static_assert(std::is_same_v<decltype(fromRaylib(::Texture{})), sprout::Texture>);

TEST_CASE("Vec2 <-> raylib::Vector2 round-trips exactly") {
    const std::vector<sprout::Vec2> values{
        {0, 0}, {1, 2}, {-3.25f, 4.5f}, {0.1f, -0.7f}, {123456.789f, -98765.4321f}, {1e-30f, -1e30f},
    };
    for (const sprout::Vec2& v : values) {
        CAPTURE(v);
        const raylib::Vector2 r = toRaylib(v);
        CHECK(r.x == v.x);
        CHECK(r.y == v.y);
        CHECK(fromRaylib(r) == v);
    }
    SUBCASE("from a plain raylib struct") {
        const ::Vector2 plain{7.5f, -8.25f};
        CHECK(fromRaylib(plain) == sprout::Vec2{7.5f, -8.25f});
    }
    SUBCASE("raylib-cpp methods work on the result") {
        CHECK(toRaylib(sprout::Vec2{3, 4}).Length() == Approx(5.0f));
    }
}

TEST_CASE("Rect <-> raylib::Rectangle round-trips exactly") {
    const std::vector<sprout::Rect> values{
        {0, 0, 0, 0}, {10, 20, 30, 40}, {-5.5f, -6.25f, 0.1f, 1000.75f}, {1e6f, -1e6f, 3.3f, 4.4f},
    };
    for (const sprout::Rect& rect : values) {
        CAPTURE(rect);
        const raylib::Rectangle r = toRaylib(rect);
        CHECK(r.x == rect.x);
        CHECK(r.y == rect.y);
        CHECK(r.width == rect.width);
        CHECK(r.height == rect.height);
        CHECK(fromRaylib(r) == rect);
    }
    SUBCASE("raylib agrees that touching rectangles don't collide") {
        const sprout::Rect a{0, 0, 10, 10};
        const sprout::Rect overlapping{5, 5, 10, 10};
        const sprout::Rect touching{10, 0, 10, 10};
        CHECK(toRaylib(a).CheckCollision(toRaylib(overlapping)) == a.overlaps(overlapping));
        CHECK(toRaylib(a).CheckCollision(toRaylib(touching)) == a.overlaps(touching));
    }
}

TEST_CASE("Color <-> raylib::Color round-trips exactly, including alpha") {
    for (int value = 0; value <= 255; value += 15) {
        const sprout::Color c{value, 255 - value, (value * 7) % 256, value};
        CAPTURE(c);
        const raylib::Color r = toRaylib(c);
        CHECK(r.r == c.r);
        CHECK(r.g == c.g);
        CHECK(r.b == c.b);
        CHECK(r.a == c.a);
        CHECK(fromRaylib(r) == c);
    }
    CHECK(fromRaylib(toRaylib(sprout::Color::Blank)) == sprout::Color::Blank);
    CHECK(fromRaylib(toRaylib(sprout::Color::White)) == sprout::Color::White);
    CHECK(fromRaylib(toRaylib(sprout::Color{1, 2, 3, 0})) == sprout::Color{1, 2, 3, 0});
    SUBCASE("from a plain raylib color constant") {
        CHECK(fromRaylib(::Color{255, 0, 0, 128}) == sprout::Color{255, 0, 0, 128});
    }
}

TEST_CASE("Texture <-> raylib::TextureUnmanaged keeps every field") {
    // Just a "ticket" with made-up numbers: no picture is loaded and nothing
    // is unloaded when the raylib-cpp view goes away (it is unmanaged).
    const sprout::Texture texture{42, 640, 480, 3, 7};
    {
        const raylib::TextureUnmanaged r = toRaylib(texture);
        CHECK(r.id == 42u);
        CHECK(r.width == 640);
        CHECK(r.height == 480);
        CHECK(r.mipmaps == 3);
        CHECK(r.format == 7);

        const sprout::Texture back = fromRaylib(r);
        CHECK(back.id == texture.id);
        CHECK(back.width == texture.width);
        CHECK(back.height == texture.height);
        CHECK(back.mipmaps == texture.mipmaps);
        CHECK(back.format == texture.format);
        CHECK(back.isValid());
        CHECK(back.size() == sprout::Vec2{640, 480});
    }
    SUBCASE("an empty texture stays empty") {
        const sprout::Texture empty;
        const sprout::Texture back = fromRaylib(toRaylib(empty));
        CHECK_FALSE(back.isValid());
        CHECK(back.width == 0);
        CHECK(back.height == 0);
        CHECK(back.mipmaps == 0);
        CHECK(back.format == 0);
    }
}

TEST_CASE("Camera -> raylib::Camera2D (without a window)") {
    sprout::Camera camera;
    camera.position = {123.5f, -45.25f};
    camera.zoom = 2.5f;
    camera.rotation = 30.0f;

    const raylib::Camera2D view = toRaylib(camera);
    CHECK(view.target.x == 123.5f);
    CHECK(view.target.y == -45.25f);
    CHECK(view.zoom == 2.5f);
    CHECK(view.rotation == 30.0f);
    // The offset is the middle of the screen (no window here, so 0,0) plus any screen shake.
    CHECK(fromRaylib(view.offset) == sprout::screenCenter() + camera.shakeOffset());

    SUBCASE("a zoom of 0 or less is made a tiny positive number (raylib can't divide by 0)") {
        camera.zoom = 0.0f;
        CHECK(toRaylib(camera).zoom > 0.0f);
        camera.zoom = -1.0f;
        CHECK(toRaylib(camera).zoom > 0.0f);
    }
    SUBCASE("screen shake moves the offset") {
        sprout::setRandomSeed(1);
        camera.shake(10.0f, 1.0f);
        camera.updateCamera(0.1f);
        const raylib::Camera2D shaken = toRaylib(camera);
        CHECK(fromRaylib(shaken.offset) == sprout::screenCenter() + camera.shakeOffset());
        CHECK(camera.shakeOffset().length() <= 10.0f);
    }
}
