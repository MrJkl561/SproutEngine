// Tests for sprout/particles.hpp (everything except draw(), which needs a window).
#include <doctest/doctest.h>

#include <cmath>
#include <vector>

#include <sprout/particles.hpp>
#include <sprout/random.hpp>

#include "test_helpers.hpp"

using namespace sprout;
using doctest::Approx;

// -----------------------------------------------------------------------------
//  ParticleSystem only tells you count(): there is no public way to look at a
//  single particle. To test movement, gravity and drag anyway, the tests peek
//  at the private list with a well-known, standard C++ trick (access checks
//  don't apply to explicit template instantiations). If the engine ever gets a
//  read-only getter for its particles, use that here instead.
// -----------------------------------------------------------------------------
namespace {
using ParticleList = std::vector<Particle>;

template <ParticleList ParticleSystem::*Member>
struct PeekInside {
    friend const ParticleList& particlesOf(const ParticleSystem& system) { return system.*Member; }
};
template struct PeekInside<&ParticleSystem::particles_>;
const ParticleList& particlesOf(const ParticleSystem& system);

/// One particle at {100, 100} that lives for `life` seconds.
Particle makeParticle(float life, Vec2 velocity = {}) {
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
    CHECK(particles.gravity == 0.0f);
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

TEST_CASE("burst() particles start at the position and fly out in random directions") {
    setRandomSeed(2);
    ParticleSystem particles;
    particles.burst({400, 300}, Color::Gold, 200, 150.0f, 0.6f, 4.0f);
    bool left = false, right = false, up = false, down = false;
    for (const Particle& p : particlesOf(particles)) {
        REQUIRE(p.position == Vec2{400, 300});
        REQUIRE(p.color == Color::Gold);
        const float speed = p.velocity.length();
        REQUIRE(speed >= Approx(150.0f * 0.3f));
        REQUIRE(speed <= Approx(150.0f));
        REQUIRE(p.life >= Approx(0.6f * 0.6f));
        REQUIRE(p.life <= Approx(0.6f));
        REQUIRE(p.maxLife == p.life);
        REQUIRE(p.size >= Approx(4.0f * 0.6f));
        REQUIRE(p.size <= Approx(4.0f * 1.2f));
        left = left || p.velocity.x < 0;
        right = right || p.velocity.x > 0;
        up = up || p.velocity.y < 0;
        down = down || p.velocity.y > 0;
    }
    CHECK((left && right && up && down));
}

TEST_CASE("spray() fires particles in one direction") {
    setRandomSeed(3);
    ParticleSystem particles;
    particles.spray({0, 0}, Color::Orange, 90.0f, 30.0f);
    CHECK(particles.count() == 5); // the default
    particles.clear();

    particles.spray({50, 60}, Color::Orange, 90.0f, 30.0f, 100, 200.0f); // down, give or take 15 degrees
    CHECK(particles.count() == 100);
    for (const Particle& p : particlesOf(particles)) {
        REQUIRE(p.position == Vec2{50, 60});
        REQUIRE(p.velocity.angle() >= Approx(75.0f));
        REQUIRE(p.velocity.angle() <= Approx(105.0f));
        REQUIRE(p.velocity.length() >= Approx(100.0f));
        REQUIRE(p.velocity.length() <= Approx(200.0f));
    }
}

TEST_CASE("add() adds one particle exactly as given") {
    ParticleSystem particles;
    Particle p = makeParticle(2.0f, Vec2{5, 6});
    p.color = Color::Lime;
    p.size = 9.0f;
    particles.add(p);
    REQUIRE(particles.count() == 1);
    const Particle& added = particlesOf(particles).front();
    CHECK(added.position == p.position);
    CHECK(added.velocity == p.velocity);
    CHECK(added.color == Color::Lime);
    CHECK(added.size == 9.0f);
    CHECK(added.life == 2.0f);
}

TEST_CASE("particles disappear when their lifetime is over") {
    setRandomSeed(4);
    ParticleSystem particles;
    particles.burst({0, 0}, Color::White, 30, 150.0f, 0.5f); // each lives 0.3 to 0.5 seconds

    particles.update(0.25f);
    CHECK(particles.count() == 30); // nobody dies that young
    particles.update(0.26f);
    CHECK(particles.count() == 0); // everybody is gone after 0.51 seconds

    SUBCASE("exactly at the end of its life a particle is removed") {
        particles.add(makeParticle(1.0f));
        particles.update(0.5f);
        CHECK(particles.count() == 1);
        CHECK(particlesOf(particles).front().life == Approx(0.5f));
        particles.update(0.5f);
        CHECK(particles.count() == 0);
    }
    SUBCASE("only the dead ones are removed") {
        particles.add(makeParticle(0.5f));
        particles.add(makeParticle(2.0f));
        particles.add(makeParticle(0.5f));
        particles.update(1.0f);
        REQUIRE(particles.count() == 1);
        CHECK(particlesOf(particles).front().maxLife == 2.0f);
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
    particles.spray({0, 0}, Color::White, 0.0f, 10.0f, 10);
    particles.add(makeParticle(1.0f));
    CHECK(particles.count() == 5000);

    SUBCASE("room is made again when old ones die") {
        particles.update(10.0f);
        CHECK(particles.count() == 0);
        particles.burst({0, 0}, Color::White, 10);
        CHECK(particles.count() == 10);
    }
}

TEST_CASE("particles move, fall with gravity and slow down with drag") {
    ParticleSystem particles;

    SUBCASE("no gravity and no drag: a straight line at constant speed") {
        particles.drag = 0.0f;
        particles.add(makeParticle(10.0f, Vec2{100, -40}));
        particles.update(0.5f);
        const Particle& p = particlesOf(particles).front();
        CHECK(p.velocity == sprout_test::ApproxVec2(100, -40));
        CHECK(p.position == sprout_test::ApproxVec2(150, 80));
    }
    SUBCASE("gravity pulls down: velocity.y grows by gravity * dt") {
        particles.drag = 0.0f;
        particles.gravity = 400.0f;
        particles.add(makeParticle(10.0f, Vec2{30, 0}));
        particles.update(0.25f);
        const Particle& p = particlesOf(particles).front();
        CHECK(p.velocity.x == Approx(30.0f)); // sideways speed is untouched
        CHECK(p.velocity.y == Approx(100.0f));
        CHECK(p.position.y > 100.0f); // moved down
        particles.update(0.25f);
        CHECK(particlesOf(particles).front().velocity.y == Approx(200.0f));
    }
    SUBCASE("negative gravity makes particles rise (smoke!)") {
        particles.drag = 0.0f;
        particles.gravity = -100.0f;
        particles.add(makeParticle(10.0f));
        particles.update(1.0f);
        CHECK(particlesOf(particles).front().velocity.y == Approx(-100.0f));
        CHECK(particlesOf(particles).front().position.y < 100.0f);
    }
    SUBCASE("drag slows particles down smoothly") {
        particles.drag = 1.0f;
        particles.add(makeParticle(10.0f, Vec2{100, 0}));
        particles.update(1.0f);
        CHECK(particlesOf(particles).front().velocity.x == Approx(100.0f * std::exp(-1.0f)));
        // The same total time in small steps slows it down the same amount.
        ParticleSystem steps;
        steps.drag = 1.0f;
        steps.add(makeParticle(10.0f, Vec2{100, 0}));
        for (int i = 0; i < 10; ++i) steps.update(0.1f);
        CHECK(particlesOf(steps).front().velocity.x == Approx(100.0f * std::exp(-1.0f)));
    }
    SUBCASE("paused particles (dt = 0) don't move or age") {
        particles.gravity = 500.0f;
        particles.add(makeParticle(1.0f, Vec2{10, 10}));
        particles.update(0.0f);
        const Particle& p = particlesOf(particles).front();
        CHECK(p.position == Vec2{100, 100});
        CHECK(p.life == 1.0f);
    }
}
