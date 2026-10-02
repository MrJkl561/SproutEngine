// Tests for sprout/sprite.hpp: sprite sheets and animations.
//
// No picture is loaded (that needs a window), but frame numbers and
// animations work exactly the same without one.
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include <sprout/sprite.hpp>

#include "test_helpers.hpp"

using namespace sprout;
using sprout_test::CaptureStderr;

namespace {
/// 8 frames per second = exactly 0.125 seconds per frame (no rounding errors).
constexpr float kFps = 8.0f;
constexpr float kFrameTime = 1.0f / kFps;

Sprite makeSheet() {
    Sprite sprite;
    sprite.setSpriteSheet(4, 2); // 4 columns, 2 rows = 8 frames
    return sprite;
}

/// Calls update() `frames` times, one animation frame's worth of time each.
void advance(Sprite& sprite, int frames) {
    for (int i = 0; i < frames; ++i) sprite.update(kFrameTime);
}
} // namespace

TEST_CASE("Sprite sheet frames") {
    Sprite sprite;
    CHECK(sprite.frameCount() == 1);
    CHECK(sprite.frame() == 0);

    sprite.setSpriteSheet(4, 2);
    CHECK(sprite.frameCount() == 8);
    sprite.setSpriteSheet(6); // one row by default
    CHECK(sprite.frameCount() == 6);
    sprite.setSpriteSheet(0, -3); // nonsense is made 1x1
    CHECK(sprite.frameCount() == 1);

    SUBCASE("setFrame picks a frame and keeps it inside the sheet") {
        Sprite sheet = makeSheet();
        sheet.setFrame(5);
        CHECK(sheet.frame() == 5);
        sheet.setFrame(100);
        CHECK(sheet.frame() == 7);
        sheet.setFrame(-5);
        CHECK(sheet.frame() == 0);
    }
    SUBCASE("making the sheet smaller keeps the current frame valid") {
        Sprite sheet = makeSheet();
        sheet.setFrame(7);
        sheet.setSpriteSheet(2, 1);
        CHECK(sheet.frame() == 1);
    }
}

TEST_CASE("Sprite without a picture") {
    Sprite sprite = makeSheet();
    CHECK_FALSE(sprite.texture().isValid());
    CHECK(sprite.width() == 0.0f);
    CHECK(sprite.height() == 0.0f);
    CHECK(sprite.size() == Vec2{0, 0});
    sprite.position = {10, 20};
    CHECK(sprite.bounds() == Rect{10, 20, 0, 0});
    CHECK_NOTHROW(sprite.draw()); // drawing nothing is fine, even without a window
    CHECK_NOTHROW(sprite.draw(Vec2{5, 5}));

    SUBCASE("loading a picture before the window exists warns instead of crashing") {
        CaptureStderr captured;
        Sprite fromFile{"assets/player.png"};
        CHECK_FALSE(fromFile.texture().isValid());
        CHECK(captured.text().find("assets/player.png") != std::string::npos);
    }
}

TEST_CASE("addAnimation with a range of frames") {
    SUBCASE("forwards: 0 to 3") {
        Sprite sprite = makeSheet();
        sprite.addAnimation("walk", 0, 3, kFps);
        sprite.play("walk");
        std::vector<int> seen{sprite.frame()};
        for (int i = 0; i < 4; ++i) {
            sprite.update(kFrameTime);
            seen.push_back(sprite.frame());
        }
        CHECK(seen == std::vector<int>{0, 1, 2, 3, 0}); // ...and it loops
    }
    SUBCASE("backwards: 7 down to 4") {
        Sprite sprite = makeSheet();
        sprite.addAnimation("rewind", 7, 4, kFps);
        sprite.play("rewind");
        std::vector<int> seen{sprite.frame()};
        for (int i = 0; i < 4; ++i) {
            sprite.update(kFrameTime);
            seen.push_back(sprite.frame());
        }
        CHECK(seen == std::vector<int>{7, 6, 5, 4, 7});
    }
    SUBCASE("a single frame") {
        Sprite sprite = makeSheet();
        sprite.addAnimation("still", 2, 2, kFps);
        sprite.play("still");
        advance(sprite, 5);
        CHECK(sprite.frame() == 2);
    }
}

TEST_CASE("addAnimation with a list of frames") {
    Sprite sprite = makeSheet();
    sprite.addAnimation("bob", {0, 1, 2, 1}, kFps);
    sprite.play("bob");
    std::vector<int> seen{sprite.frame()};
    for (int i = 0; i < 6; ++i) {
        sprite.update(kFrameTime);
        seen.push_back(sprite.frame());
    }
    CHECK(seen == std::vector<int>{0, 1, 2, 1, 0, 1, 2});

    SUBCASE("an empty list is refused with a warning") {
        CaptureStderr captured;
        sprite.addAnimation("nothing", std::vector<int>{}, kFps);
        CHECK(captured.text().find("nothing") != std::string::npos);
        sprite.play("nothing"); // doesn't exist, so "bob" keeps playing
        CHECK(sprite.animation() == "bob");
    }
}

TEST_CASE("update() advances frames at the animation's speed") {
    Sprite sprite = makeSheet();
    sprite.addAnimation("walk", 0, 7, kFps);
    sprite.play("walk");

    sprite.update(kFrameTime / 2);
    CHECK(sprite.frame() == 0); // half a frame's time: not yet
    sprite.update(kFrameTime / 2);
    CHECK(sprite.frame() == 1);

    SUBCASE("a long dt skips ahead several frames") {
        sprite.update(kFrameTime * 3);
        CHECK(sprite.frame() == 4);
    }
    SUBCASE("leftover time is kept for the next update") {
        sprite.update(kFrameTime * 0.75f);
        CHECK(sprite.frame() == 1);
        sprite.update(kFrameTime * 0.25f);
        CHECK(sprite.frame() == 2);
    }
    SUBCASE("one second (in many small steps) at 8 fps is 8 frames: all the way round") {
        for (int i = 0; i < 64; ++i) sprite.update(1.0f / 64.0f);
        CHECK(sprite.frame() == 1); // we were on frame 1, and 8 frames later we're there again
    }
    SUBCASE("different speeds") {
        Sprite fast = makeSheet();
        fast.addAnimation("fast", 0, 7, 16.0f);
        fast.play("fast");
        fast.update(0.25f);
        CHECK(fast.frame() == 4);
    }
}

TEST_CASE("play() doesn't restart an animation that is already playing, restart() does") {
    Sprite sprite = makeSheet();
    sprite.addAnimation("walk", 0, 7, kFps);
    sprite.addAnimation("run", 4, 7, kFps);

    sprite.play("walk");
    CHECK(sprite.isPlaying());
    CHECK(sprite.animation() == "walk");
    advance(sprite, 3);
    CHECK(sprite.frame() == 3);

    // Calling play("walk") every frame (the usual way) must not keep resetting it.
    for (int i = 0; i < 2; ++i) {
        sprite.play("walk");
        sprite.update(kFrameTime);
    }
    CHECK(sprite.frame() == 5);

    sprite.restart("walk");
    CHECK(sprite.frame() == 0);
    CHECK(sprite.isPlaying());

    SUBCASE("play() with another name switches to it from its first frame") {
        sprite.play("run");
        CHECK(sprite.animation() == "run");
        CHECK(sprite.frame() == 4);
    }
    SUBCASE("stop() pauses and play() carries on from the same frame") {
        advance(sprite, 2);
        sprite.stop();
        CHECK_FALSE(sprite.isPlaying());
        advance(sprite, 3);
        CHECK(sprite.frame() == 2); // stopped: nothing moves
        sprite.play("walk");
        CHECK(sprite.isPlaying());
        CHECK(sprite.frame() == 2);
        advance(sprite, 1);
        CHECK(sprite.frame() == 3);
    }
}

TEST_CASE("looping and non-looping animations") {
    Sprite sprite = makeSheet();
    sprite.addAnimation("walk", 0, 3, kFps);              // loops
    sprite.addAnimation("jump", 4, 7, kFps, false);       // plays once

    SUBCASE("a looping animation goes round and round and never finishes") {
        sprite.play("walk");
        advance(sprite, 4 * 10 + 1);
        CHECK(sprite.frame() == 1);
        CHECK(sprite.isPlaying());
        CHECK_FALSE(sprite.isAnimationFinished());
    }
    SUBCASE("a non-looping animation stops on its last frame") {
        sprite.play("jump");
        CHECK_FALSE(sprite.isAnimationFinished());
        advance(sprite, 2);
        CHECK(sprite.frame() == 6);
        CHECK_FALSE(sprite.isAnimationFinished());
        advance(sprite, 1);
        CHECK(sprite.frame() == 7);
        CHECK(sprite.isAnimationFinished()); // reached the last frame
        advance(sprite, 10);
        CHECK(sprite.frame() == 7);          // and stays there
        CHECK(sprite.isAnimationFinished());
        CHECK_FALSE(sprite.isPlaying());
        CHECK(sprite.animation() == "jump");

        SUBCASE("restart() plays it again") {
            sprite.restart("jump");
            CHECK(sprite.frame() == 4);
            CHECK_FALSE(sprite.isAnimationFinished());
            advance(sprite, 3);
            CHECK(sprite.isAnimationFinished());
        }
    }
    SUBCASE("a big dt can't run past the end of a non-looping animation") {
        sprite.play("jump");
        sprite.update(100.0f);
        CHECK(sprite.frame() == 7);
        CHECK(sprite.isAnimationFinished());
    }
}

// The usual "call play() every frame" pattern must keep a play-once animation on its last frame.
TEST_CASE("calling play() every frame doesn't replay a finished non-looping animation") {
    Sprite sprite = makeSheet();
    sprite.addAnimation("jump", 4, 7, kFps, false);
    std::vector<int> seen;
    for (int frame = 0; frame < 12; ++frame) { // like a player who stays in the air
        sprite.play("jump");
        sprite.update(kFrameTime);
        seen.push_back(sprite.frame());
    }
    CHECK(seen == std::vector<int>{5, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7});
    CHECK(sprite.isAnimationFinished());
    CHECK_FALSE(sprite.isPlaying());

    SUBCASE("restart() is how you replay it on purpose") {
        sprite.restart("jump");
        CHECK(sprite.frame() == 4);
        CHECK(sprite.isPlaying());
        CHECK_FALSE(sprite.isAnimationFinished());
    }
}

TEST_CASE("setFrame stops the animation") {
    Sprite sprite = makeSheet();
    sprite.addAnimation("walk", 0, 7, kFps);
    sprite.play("walk");
    advance(sprite, 2);
    sprite.setFrame(6);
    CHECK_FALSE(sprite.isPlaying());
    CHECK(sprite.animation().empty());
    CHECK_FALSE(sprite.isAnimationFinished());
    advance(sprite, 5);
    CHECK(sprite.frame() == 6);
}

TEST_CASE("unknown animation names don't crash") {
    Sprite sprite = makeSheet();
    sprite.addAnimation("walk", 0, 3, kFps);
    sprite.play("walk");
    advance(sprite, 1);

    CaptureStderr captured;
    CHECK_NOTHROW(sprite.play("fly"));
    CHECK_NOTHROW(sprite.restart("swim"));
    CHECK(captured.text().find("fly") != std::string::npos);
    CHECK(captured.text().find("swim") != std::string::npos);
    // The animation that was playing just carries on.
    CHECK(sprite.animation() == "walk");
    CHECK(sprite.isPlaying());
    advance(sprite, 1);
    CHECK(sprite.frame() == 2);

    SUBCASE("updating a sprite that never had any animation is harmless") {
        Sprite plain = makeSheet();
        plain.setFrame(3);
        CHECK_NOTHROW(plain.update(1.0f));
        CHECK(plain.frame() == 3);
        CHECK_FALSE(plain.isAnimationFinished());
    }
}

TEST_CASE("animation frames outside the sprite sheet are clamped") {
    Sprite sprite = makeSheet(); // 8 frames
    sprite.addAnimation("tooFar", 6, 10, kFps);
    sprite.play("tooFar");
    advance(sprite, 4);
    CHECK(sprite.frame() >= 0);
    CHECK(sprite.frame() <= 7);
}

// Replacing the animation that is playing starts it again from its (new) first frame.
TEST_CASE("redefining the playing animation with fewer frames stays inside the new frames") {
    Sprite sprite = makeSheet();
    sprite.addAnimation("walk", 0, 7, kFps);
    sprite.play("walk");
    advance(sprite, 6); // on the 7th frame of 8
    REQUIRE(sprite.frame() == 6);

    sprite.addAnimation("walk", {0, 1}, kFps); // now only 2 frames
    CHECK(sprite.frame() == 0);
    sprite.update(kFrameTime / 10);            // not enough time to move on a frame
    CHECK(sprite.frame() == 0);
    sprite.update(kFrameTime);
    CHECK(sprite.frame() == 1);
    sprite.update(kFrameTime);
    CHECK(sprite.frame() == 0); // and it loops over the new frames
    CHECK(sprite.isPlaying());
}
