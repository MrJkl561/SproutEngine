// Tests for sprout/math.hpp: Vec2, Rect and the helper functions.
#include <doctest/doctest.h>

#include <type_traits>

#include <sprout/math.hpp>

#include "test_helpers.hpp"

using namespace sprout;
using doctest::Approx;
using sprout_test::ApproxVec2;

// -----------------------------------------------------------------------------
//  Compile-time checks: beginners write Vec2{x, y} and Rect{x, y, w, h} with
//  int variables all the time. That must compile (no "narrowing conversion"
//  errors) and work in constexpr code.
// -----------------------------------------------------------------------------
namespace {
constexpr int kIntX = 3;
constexpr int kIntY = 4;
constexpr Vec2 kFromInts{kIntX, kIntY};
static_assert(kFromInts.x == 3.0f && kFromInts.y == 4.0f);
constexpr Vec2 kFromDoubles{1.5, 2.5};
static_assert(kFromDoubles.x == 1.5f && kFromDoubles.y == 2.5f);
constexpr Vec2 kMixed{1, 2.5f};
static_assert(kMixed.x == 1.0f && kMixed.y == 2.5f);
constexpr Rect kRectFromInts{kIntX, kIntY, 10, 20};
static_assert(kRectFromInts.right() == 13.0f && kRectFromInts.bottom() == 24.0f);
static_assert(Vec2{} == Vec2{0, 0});
static_assert(Vec2{1, 2} + Vec2{3, 4} == Vec2{4, 6});
static_assert(Rect::fromCenter({50, 50}, {20, 10}) == Rect{40, 45, 20, 10});
static_assert(Rect{0, 0, 10, 10}.overlaps(Rect{5, 5, 10, 10}));
static_assert(!Rect{0, 0, 10, 10}.overlaps(Rect{10, 0, 10, 10}));
static_assert(clamp(150.0f, 0.0f, 100.0f) == 100.0f);
static_assert(std::is_trivially_copyable_v<Vec2>);
static_assert(std::is_trivially_copyable_v<Rect>);
} // namespace

TEST_CASE("Vec2 can be made from ints, floats, doubles and size_t variables") {
    int x = 7;
    int y = -2;
    const Vec2 a{x, y};
    CHECK(a.x == 7.0f);
    CHECK(a.y == -2.0f);

    std::size_t count = 5;
    const Vec2 b{count, 1.25};
    CHECK(b.x == 5.0f);
    CHECK(b.y == 1.25f);

    const Vec2 zero;
    CHECK(zero == Vec2{0, 0});
}

TEST_CASE("Vec2 arithmetic operators") {
    const Vec2 a{1, 2};
    const Vec2 b{3, 5};

    CHECK(a + b == Vec2{4, 7});
    CHECK(b - a == Vec2{2, 3});
    CHECK(a * b == Vec2{3, 10});
    CHECK(Vec2{6, 10} / Vec2{2, 5} == Vec2{3, 2});
    CHECK(a * 2 == Vec2{2, 4});
    CHECK(2 * a == Vec2{2, 4});
    CHECK(Vec2{4, 8} / 2 == Vec2{2, 4});
    CHECK(-a == Vec2{-1, -2});

    SUBCASE("compound assignment") {
        Vec2 v{1, 1};
        v += Vec2{2, 3};
        CHECK(v == Vec2{3, 4});
        v -= Vec2{1, 1};
        CHECK(v == Vec2{2, 3});
        v *= 3;
        CHECK(v == Vec2{6, 9});
        v /= 3;
        CHECK(v == Vec2{2, 3});
    }

    SUBCASE("equality") {
        CHECK(Vec2{1, 2} == Vec2{1.0f, 2.0f});
        CHECK(Vec2{1, 2} != Vec2{2, 1});
    }
}

TEST_CASE("Vec2 length, lengthSquared and normalized") {
    CHECK(Vec2{3, 4}.length() == Approx(5.0f));
    CHECK(Vec2{3, 4}.lengthSquared() == Approx(25.0f));
    CHECK(Vec2{}.length() == 0.0f);

    SUBCASE("normalized keeps the direction with length 1") {
        const Vec2 n = Vec2{3, 4}.normalized();
        CHECK(n == ApproxVec2(0.6, 0.8));
        CHECK(n.length() == Approx(1.0f));
        CHECK(Vec2{0, -10}.normalized() == ApproxVec2(0, -1));
    }

    SUBCASE("normalizing the zero vector gives zero instead of NaN") {
        const Vec2 n = Vec2{}.normalized();
        CHECK(n.x == 0.0f);
        CHECK(n.y == 0.0f);
    }
}

TEST_CASE("Vec2 distanceTo and directionTo") {
    const Vec2 a{1, 1};
    const Vec2 b{4, 5};
    CHECK(a.distanceTo(b) == Approx(5.0f));
    CHECK(b.distanceTo(a) == Approx(5.0f));
    CHECK(a.distanceTo(a) == 0.0f);

    CHECK(a.directionTo(b) == ApproxVec2(0.6, 0.8));
    CHECK(a.directionTo(b).length() == Approx(1.0f));
    CHECK(a.directionTo(a) == Vec2{0, 0}); // same point: no direction, but no NaN either
}

TEST_CASE("Vec2 angles are in degrees, 0 = right and 90 = down") {
    CHECK(Vec2{1, 0}.angle() == Approx(0.0f));
    CHECK(Vec2{0, 1}.angle() == Approx(90.0f));
    CHECK(Vec2{-1, 0}.angle() == Approx(180.0f));
    CHECK(Vec2{0, -1}.angle() == Approx(-90.0f));
    CHECK(Vec2{1, 1}.angle() == Approx(45.0f));

    SUBCASE("angleTo is the angle of the line between two points") {
        CHECK(Vec2{10, 10}.angleTo(Vec2{20, 10}) == Approx(0.0f));
        CHECK(Vec2{10, 10}.angleTo(Vec2{10, 30}) == Approx(90.0f));
        CHECK(Vec2{10, 10}.angleTo(Vec2{0, 0}) == Approx(-135.0f));
    }
}

TEST_CASE("Vec2::rotated turns clockwise on screen") {
    CHECK(Vec2{1, 0}.rotated(90) == ApproxVec2(0, 1));
    CHECK(Vec2{1, 0}.rotated(180) == ApproxVec2(-1, 0));
    CHECK(Vec2{1, 0}.rotated(-90) == ApproxVec2(0, -1));
    CHECK(Vec2{2, 3}.rotated(360) == ApproxVec2(2, 3));
    CHECK(Vec2{0, 5}.rotated(90) == ApproxVec2(-5, 0));
    // Rotating never changes the length.
    CHECK(Vec2{3, 4}.rotated(33).length() == Approx(5.0f));
}

TEST_CASE("Vec2::fromAngle makes a vector from degrees and a length") {
    CHECK(Vec2::fromAngle(0) == ApproxVec2(1, 0));
    CHECK(Vec2::fromAngle(90, 5) == ApproxVec2(0, 5));
    CHECK(Vec2::fromAngle(180, 2) == ApproxVec2(-2, 0));
    CHECK(Vec2::fromAngle(-90) == ApproxVec2(0, -1));
    // fromAngle and angle() are opposites.
    CHECK(Vec2::fromAngle(37, 10).angle() == Approx(37.0f));
}

TEST_CASE("Vec2::dot tells if two directions agree") {
    CHECK(Vec2{1, 2}.dot(Vec2{3, 4}) == Approx(11.0f));
    CHECK(Vec2{1, 0}.dot(Vec2{0, 1}) == 0.0f);  // at right angles
    CHECK(Vec2{1, 0}.dot(Vec2{-1, 0}) < 0.0f);  // opposite
    CHECK(Vec2{1, 0}.dot(Vec2{1, 1}) > 0.0f);   // same-ish
}

TEST_CASE("Rect basics: corners, edges, center and size") {
    const Rect r{10, 20, 30, 40};
    CHECK(r.left() == 10.0f);
    CHECK(r.right() == 40.0f);
    CHECK(r.top() == 20.0f);
    CHECK(r.bottom() == 60.0f);
    CHECK(r.position() == Vec2{10, 20});
    CHECK(r.size() == Vec2{30, 40});
    CHECK(r.center() == Vec2{25, 40});

    SUBCASE("Rect from a top-left point and a size") {
        CHECK(Rect{Vec2{10, 20}, Vec2{30, 40}} == r);
    }
    SUBCASE("Rect::fromCenter puts the middle at the given point") {
        const Rect c = Rect::fromCenter({100, 100}, {20, 40});
        CHECK(c == Rect{90, 80, 20, 40});
        CHECK(c.center() == Vec2{100, 100});
    }
    SUBCASE("setCenter moves the rectangle without resizing it") {
        Rect moved = r;
        moved.setCenter({0, 0});
        CHECK(moved == Rect{-15, -20, 30, 40});
        CHECK(moved.center() == Vec2{0, 0});
    }
    SUBCASE("a default Rect is empty at 0,0") {
        CHECK(Rect{} == Rect{0, 0, 0, 0});
    }
}

TEST_CASE("Rect::overlaps - touching edges do NOT count as overlapping") {
    const Rect a{0, 0, 10, 10};

    CHECK(a.overlaps(Rect{5, 5, 10, 10}));
    CHECK(a.overlaps(Rect{2, 2, 2, 2}));    // completely inside
    CHECK(a.overlaps(Rect{-5, -5, 20, 20})); // completely around
    CHECK(a.overlaps(a));

    SUBCASE("touching edges") {
        CHECK_FALSE(a.overlaps(Rect{10, 0, 10, 10}));  // right edge
        CHECK_FALSE(a.overlaps(Rect{-10, 0, 10, 10})); // left edge
        CHECK_FALSE(a.overlaps(Rect{0, 10, 10, 10}));  // bottom edge
        CHECK_FALSE(a.overlaps(Rect{0, -10, 10, 10})); // top edge
        CHECK_FALSE(a.overlaps(Rect{10, 10, 5, 5}));   // only a corner
    }
    SUBCASE("far apart") {
        CHECK_FALSE(a.overlaps(Rect{50, 50, 10, 10}));
        CHECK_FALSE(a.overlaps(Rect{0, 11, 10, 10}));
    }
    SUBCASE("overlap works both ways") {
        const Rect b{9.5f, 9.5f, 1, 1};
        CHECK(a.overlaps(b));
        CHECK(b.overlaps(a));
    }
}

TEST_CASE("Rect::contains includes the top-left edge but not the bottom-right edge") {
    const Rect r{0, 0, 10, 10};
    CHECK(r.contains({5, 5}));
    CHECK(r.contains({0, 0}));
    CHECK(r.contains({9.99f, 9.99f}));
    CHECK_FALSE(r.contains({10, 5}));
    CHECK_FALSE(r.contains({5, 10}));
    CHECK_FALSE(r.contains({-0.01f, 5}));
    CHECK_FALSE(r.contains({50, 50}));
}

TEST_CASE("Rect::grown makes a rectangle bigger (or smaller) on every side") {
    const Rect r{10, 10, 20, 20};
    CHECK(r.grown(5) == Rect{5, 5, 30, 30});
    CHECK(r.grown(-5) == Rect{15, 15, 10, 10});
    CHECK(r.grown(5).center() == r.center());
    CHECK(r.grown(0) == r);
}

TEST_CASE("clamp keeps a value between min and max") {
    CHECK(clamp(150.0f, 0.0f, 100.0f) == 100.0f);
    CHECK(clamp(-5.0f, 0.0f, 100.0f) == 0.0f);
    CHECK(clamp(42.0f, 0.0f, 100.0f) == 42.0f);
    CHECK(clamp(0.0f, 0.0f, 100.0f) == 0.0f);
    CHECK(clamp(100.0f, 0.0f, 100.0f) == 100.0f);

    SUBCASE("clamping a point into a rectangle") {
        const Rect area{0, 0, 800, 600};
        CHECK(clamp(Vec2{900, -20}, area) == Vec2{800, 0});
        CHECK(clamp(Vec2{400, 300}, area) == Vec2{400, 300});
        CHECK(clamp(Vec2{-1, 700}, area) == Vec2{0, 600});
    }
}

TEST_CASE("lerp blends between two values") {
    SUBCASE("floats (this is std::lerp)") {
        CHECK(lerp(0.0f, 10.0f, 0.0f) == Approx(0.0f));
        CHECK(lerp(0.0f, 10.0f, 1.0f) == Approx(10.0f));
        CHECK(lerp(0.0f, 10.0f, 0.5f) == Approx(5.0f));
        CHECK(lerp(10.0f, 20.0f, 0.25f) == Approx(12.5f));
        CHECK(sprout::lerp(100.0f, 0.0f, 0.75f) == Approx(25.0f));
    }
    SUBCASE("points") {
        const Vec2 a{0, 0};
        const Vec2 b{10, 20};
        CHECK(lerp(a, b, 0.0f) == a);
        CHECK(lerp(a, b, 1.0f) == b);
        CHECK(lerp(a, b, 0.5f) == ApproxVec2(5, 10));
        CHECK(sprout::lerp(Vec2{10, 10}, Vec2{20, 0}, 0.25f) == ApproxVec2(12.5, 7.5));
    }
}

TEST_CASE("moveToward moves by at most maxStep and never overshoots") {
    SUBCASE("floats") {
        CHECK(moveToward(0.0f, 10.0f, 3.0f) == Approx(3.0f));
        CHECK(moveToward(10.0f, 0.0f, 3.0f) == Approx(7.0f));
        CHECK(moveToward(9.0f, 10.0f, 3.0f) == 10.0f);  // would overshoot: lands exactly on target
        CHECK(moveToward(11.0f, 10.0f, 3.0f) == 10.0f);
        CHECK(moveToward(10.0f, 10.0f, 3.0f) == 10.0f);
        CHECK(moveToward(-5.0f, 5.0f, 2.0f) == Approx(-3.0f));
    }
    SUBCASE("points") {
        CHECK(moveToward(Vec2{0, 0}, Vec2{10, 0}, 3.0f) == ApproxVec2(3, 0));
        CHECK(moveToward(Vec2{0, 0}, Vec2{3, 4}, 2.5f) == ApproxVec2(1.5, 2.0)); // halfway along the line
        CHECK(moveToward(Vec2{0, 0}, Vec2{3, 4}, 5.0f) == Vec2{3, 4});
        CHECK(moveToward(Vec2{0, 0}, Vec2{3, 4}, 100.0f) == Vec2{3, 4});
        CHECK(moveToward(Vec2{7, 7}, Vec2{7, 7}, 1.0f) == Vec2{7, 7}); // already there: no NaN
    }
}

TEST_CASE("wrap wraps a value around like Pac-Man") {
    CHECK(wrap(810.0f, 0.0f, 800.0f) == Approx(10.0f));
    CHECK(wrap(400.0f, 0.0f, 800.0f) == Approx(400.0f));
    CHECK(wrap(800.0f, 0.0f, 800.0f) == Approx(0.0f));
    CHECK(wrap(0.0f, 0.0f, 800.0f) == Approx(0.0f));
    CHECK(wrap(1650.0f, 0.0f, 800.0f) == Approx(50.0f));

    SUBCASE("a tiny negative value never comes out as exactly max") {
        // -0.000001 + 800 rounds to 800.0 in a float, which is the same spot as 0.
        CHECK(wrap(-1e-6f, 0.0f, 800.0f) < 800.0f);
        CHECK(wrap(-1e-6f, 0.0f, 800.0f) >= 0.0f);
    }
    SUBCASE("negative values come back in from the other side") {
        CHECK(wrap(-10.0f, 0.0f, 800.0f) == Approx(790.0f));
        CHECK(wrap(-810.0f, 0.0f, 800.0f) == Approx(790.0f));
        CHECK(wrap(-800.0f, 0.0f, 800.0f) == Approx(0.0f));
    }
    SUBCASE("ranges that don't start at zero") {
        CHECK(wrap(370.0f, 0.0f, 360.0f) == Approx(10.0f));
        CHECK(wrap(190.0f, -180.0f, 180.0f) == Approx(-170.0f));
        CHECK(wrap(-190.0f, -180.0f, 180.0f) == Approx(170.0f));
        CHECK(wrap(12.0f, 10.0f, 20.0f) == Approx(12.0f));
        CHECK(wrap(25.0f, 10.0f, 20.0f) == Approx(15.0f));
        CHECK(wrap(5.0f, 10.0f, 20.0f) == Approx(15.0f));
    }
    SUBCASE("an empty or backwards range gives min instead of dividing by zero") {
        CHECK(wrap(5.0f, 10.0f, 10.0f) == 10.0f);
        CHECK(wrap(5.0f, 10.0f, 0.0f) == 10.0f);
    }
    SUBCASE("the result is always inside [min, max)") {
        for (float value = -2000.0f; value <= 2000.0f; value += 37.25f) {
            const float wrapped = wrap(value, 0.0f, 800.0f);
            CHECK(wrapped >= 0.0f);
            CHECK(wrapped < 800.0f);
        }
    }
}

TEST_CASE("toRadians and toDegrees") {
    CHECK(toRadians(180.0f) == Approx(Pi));
    CHECK(toRadians(90.0f) == Approx(Pi / 2.0f));
    CHECK(toRadians(0.0f) == 0.0f);
    CHECK(toDegrees(Pi) == Approx(180.0f));
    CHECK(toDegrees(Pi / 4.0f) == Approx(45.0f));
    CHECK(toDegrees(toRadians(123.0f)) == Approx(123.0f));
    static_assert(toRadians(180.0f) > 3.1415f && toRadians(180.0f) < 3.1417f, "toRadians works at compile time");
}
