// Tests for sprout/timer.hpp.
//
// Timer and Cooldown use the engine's game clock, which the main loop moves
// forward every frame. Here we move it ourselves with
// sprout::detail::advanceGameClock(seconds), so the tests are instant and exact.
// The clock is shared by every test and never goes backwards, so each test
// only looks at how much time passes *during* the test.
#include <doctest/doctest.h>

#include <sprout/game.hpp>
#include <sprout/timer.hpp>

#include "test_helpers.hpp"

using namespace sprout;
using doctest::Approx;

namespace {
/// Moves game time forward like the game loop does: in steps of one frame.
void playFrames(int frames, float dt = 1.0f / 60.0f) {
    for (int i = 0; i < frames; ++i) detail::advanceGameClock(dt);
}
} // namespace

TEST_CASE("the game clock only moves when told to") {
    const double before = detail::gameClock();
    detail::advanceGameClock(0.5);
    CHECK(detail::gameClock() - before == Approx(0.5));
    CHECK_FALSE(detail::hasScene()); // no Game is running in the tests
}

TEST_CASE("Timer is done after its duration") {
    Timer timer{2.0f};
    CHECK(timer.duration() == 2.0f);
    CHECK_FALSE(timer.isDone());
    CHECK(timer.elapsed() == Approx(0.0f));
    CHECK(timer.timeLeft() == Approx(2.0f));
    CHECK(timer.progress() == Approx(0.0f));

    detail::advanceGameClock(0.5);
    CHECK_FALSE(timer.isDone());
    CHECK(timer.elapsed() == Approx(0.5f));
    CHECK(timer.timeLeft() == Approx(1.5f));
    CHECK(timer.progress() == Approx(0.25f));

    detail::advanceGameClock(1.0);
    CHECK_FALSE(timer.isDone());
    CHECK(timer.progress() == Approx(0.75f));

    detail::advanceGameClock(0.5);
    CHECK(timer.isDone()); // exactly 2 seconds
    CHECK(timer.timeLeft() == 0.0f);
    CHECK(timer.progress() == Approx(1.0f));

    SUBCASE("timeLeft and progress don't go past the end") {
        detail::advanceGameClock(10.0);
        CHECK(timer.isDone());
        CHECK(timer.timeLeft() == 0.0f);
        CHECK(timer.progress() == 1.0f);
        CHECK(timer.elapsed() == Approx(12.0f));
    }
    SUBCASE("restart() starts counting from zero again") {
        timer.restart();
        CHECK_FALSE(timer.isDone());
        CHECK(timer.elapsed() == Approx(0.0f));
        CHECK(timer.timeLeft() == Approx(2.0f));
        detail::advanceGameClock(2.0);
        CHECK(timer.isDone());
    }
    SUBCASE("restart(seconds) also changes the length") {
        timer.restart(0.5f);
        CHECK(timer.duration() == 0.5f);
        CHECK_FALSE(timer.isDone());
        CHECK(timer.timeLeft() == Approx(0.5f));
        detail::advanceGameClock(0.25);
        CHECK(timer.progress() == Approx(0.5f));
        detail::advanceGameClock(0.25);
        CHECK(timer.isDone());
    }
}

TEST_CASE("Timer counts real frames correctly") {
    Timer timer{1.0f};
    playFrames(59);
    CHECK_FALSE(timer.isDone());
    playFrames(1);
    CHECK(timer.isDone()); // 60 frames at 60 fps = 1 second
}

TEST_CASE("Timer with zero or negative seconds is done straight away") {
    Timer zero{0.0f};
    CHECK(zero.isDone());
    CHECK(zero.progress() == 1.0f);
    CHECK(zero.timeLeft() == 0.0f);

    Timer negative{-3.0f};
    CHECK(negative.duration() == 0.0f);
    CHECK(negative.isDone());
}

TEST_CASE("Timer default length is 1 second") {
    Timer timer;
    CHECK(timer.duration() == 1.0f);
}

TEST_CASE("Cooldown starts ready, and use() starts the wait") {
    Cooldown gun{0.25f};
    CHECK(gun.duration() == 0.25f);
    CHECK(gun.isReady());
    CHECK(gun.timeLeft() == 0.0f);

    CHECK(gun.use()); // first shot is allowed straight away
    CHECK_FALSE(gun.isReady());
    CHECK(gun.timeLeft() == Approx(0.25f));
    CHECK_FALSE(gun.use()); // ...but not a second one
    CHECK_FALSE(gun.use());

    detail::advanceGameClock(0.125);
    CHECK_FALSE(gun.isReady());
    CHECK(gun.timeLeft() == Approx(0.125f));
    CHECK_FALSE(gun.use()); // a failed use() doesn't restart the wait

    detail::advanceGameClock(0.125); // exactly 0.25 seconds after use()
    CHECK(gun.isReady());
    CHECK(gun.timeLeft() == 0.0f);
    CHECK(gun.use());
    CHECK_FALSE(gun.isReady());
}

TEST_CASE("Cooldown::reset makes it ready immediately") {
    Cooldown dash{5.0f};
    CHECK(dash.use());
    CHECK_FALSE(dash.isReady());
    dash.reset();
    CHECK(dash.isReady());
    CHECK(dash.timeLeft() == 0.0f);
    CHECK(dash.use());
}

TEST_CASE("Cooldown::setDuration changes how long the wait is") {
    Cooldown cooldown{10.0f};
    cooldown.setDuration(1.0f);
    CHECK(cooldown.duration() == 1.0f);
    CHECK(cooldown.use());
    detail::advanceGameClock(1.0);
    CHECK(cooldown.isReady());
}

TEST_CASE("Cooldown at 4 shots per second allows 4 shots in one second of frames") {
    Cooldown gun{0.25f};
    int shots = 0;
    for (int frame = 0; frame < 60; ++frame) {
        if (gun.use()) ++shots; // holding the fire button every frame
        detail::advanceGameClock(1.0f / 60.0f);
    }
    CHECK(shots == 4);
}

TEST_CASE("Cooldown and Timer agree on when exactly the time is up") {
    // A game running at 10 fps (dt = 0.1f) with a 0.1 second cooldown should be
    // able to fire every frame, just like a 0.1 second Timer is done every frame.
    // Start from an "awkward" clock value, like a game that has been running a while,
    // to make sure rounding errors don't make either of them a frame late.
    detail::advanceGameClock(1234.567);
    Cooldown cooldown{0.1f};
    Timer timer{0.1f};
    int cooldownReady = 0;
    int timerDone = 0;
    for (int frame = 0; frame < 100; ++frame) {
        if (cooldown.use()) ++cooldownReady;
        if (timer.isDone()) {
            ++timerDone;
            timer.restart();
        }
        detail::advanceGameClock(0.1f);
    }
    CHECK(timerDone == 99); // the first frame is at time 0
    CHECK(cooldownReady == 100);
}

TEST_CASE("Cooldown::timeLeft reaching 0 means it is ready") {
    detail::advanceGameClock(98.7654321);
    for (const float duration : {0.1f, 0.25f, 1.0f / 60.0f, 0.3f, 2.0f}) {
        CAPTURE(duration);
        Cooldown cooldown{duration};
        REQUIRE(cooldown.use());
        int frames = 0;
        while (cooldown.timeLeft() > 0.0f && frames < 1000) {
            detail::advanceGameClock(1.0f / 60.0f);
            ++frames;
        }
        CHECK(cooldown.isReady());
    }
}
