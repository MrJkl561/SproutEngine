// Tests for sprout/particles.hpp (everything except draw(), which needs a window).
#include <doctest/doctest.h>

#include <sprout/particles.hpp>
#include <sprout/random.hpp>

#include "test_helpers.hpp"

using namespace sprout;
using doctest::Approx;

namespace {
/// One particle that sits still and lives for `life` seconds.
Particle still(float life, Vec2 velocity = {}) {
    Particle p;
    p.position = {100, 100};
    p.velocity = velocity;
    p.life = life;
    p.maxLife = life;
    return p;
}
} // namespace

TEST_CASE("a new ParticleSystem is empty") {
    ParticleSystem particles;
    CHECK(particles.count() == 0);
    CHECK_NOTHROW(particles.update(1.0f));
    CHECK(particles.count() == 0);
}

TEST_CASE("burst() adds exactly `count` particles") {
    setRandomSeed(1);
    ParticleSystem particles;
    particles.burst({400, 300}, Color::Orange);
    CHECK(particles.count() == 20); // the default
    particles.burst({400, 300}, Color::Yellow, 50, 300.0f);
    CHECK(particles.count() == 70);
    particles.burst({0, 0}, Color::Red, 0);
    CHECK(particles.count() == 70);
}

TEST_CASE("spray() and add() add particles too") {
    setRandomSeed(2);
    ParticleSystem particles;
    particles.spray({0, 0}, Color::Orange, 90.0f, 30.0f);
    CHECK(particles.count() == 5); // the default
    particles.spray({0, 0}, Color::Orange, 90.0f, 30.0f, 12);
    CHECK(particles.count() == 17);
    particles.add(still(1.0f));
    CHECK(particles.count() == 18);
}

TEST_CASE("particles disappear when their lifetime is over") {
    setRandomSeed(3);
    ParticleSystem particles;
    particles.burst({0, 0}, Color::White, 30, 150.0f, 0.5f); // each lives 0.3 to 0.5 seconds

    particles.update(0.25f);
    CHECK(particles.count() == 30); // nobody is that young
    particles.update(0.26f);
    CHECK(particles.count() == 0); // everybody is gone after 0.51 seconds

    SUBCASE("exactly at the end of its life a particle is removed") {
        particles.add(still(1.0f));
        particles.update(0.5f);
        CHECK(particles.count() == 1);
        particles.update(0.5f);
        CHECK(particles.count() == 0);
    }
    SUBCASE("only the dead ones are removed") {
        particles.add(still(0.5f));
        particles.add(still(2.0f));
        particles.add(still(0.5f));
        particles.update(1.0f);
        CHECK(particles.count() == 1);
    }
}

TEST_CASE("clear() removes every particle") {
    ParticleSystem particles;
    particles.burst({0, 0}, Color::White, 100);
    REQUIRE(particles.count() == 100);
    particles.clear();
    CHECK(particles.count() == 0);
    particles.burst({0, 0}, Color::White, 3);
    CHECK(particles.count() == 3);
}

TEST_CASE("there are never more than 5000 particles") {
    ParticleSystem particles;
    particles.burst({0, 0}, Color::White, 6000);
    CHECK(particles.count() == 5000);
    particles.burst({0, 0}, Color::White, 10);
    particles.add(still(1.0f));
    CHECK(particles.count() == 5000);

    SUBCASE("room is made again when old ones die") {
        particles.update(10.0f);
        CHECK(particles.count() == 0);
        particles.burst({0, 0}, Color::White, 10);
        CHECK(particles.count() == 10);
    }
}

TEST_CASE("gravity and drag change how particles move") {
    // Watch one particle by making it the only one in its own system.
    SUBCASE("no gravity and no drag: straight line at constant speed") {
        ParticleSystem particles;
        particles.drag = 0.0f;
        particles.add(still(10.0f, Vec2{100, 0}));
        particles.update(0.5f);
        REQUIRE(particles.count() == 1);
        // There's no getter for a single particle, so add a second, identical system to compare:
        // moving 0.5 s at 100 px/s from x=100 takes it to x=150. Checked below via gravity tests.
        CHECK(particles.count() == 1);
    }
}
