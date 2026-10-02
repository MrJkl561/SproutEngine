// Tests for sprout/random.hpp.
#include <doctest/doctest.h>

#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <sprout/random.hpp>

#include "test_helpers.hpp"

using namespace sprout;
using doctest::Approx;

TEST_CASE("randomInt includes BOTH ends of the range") {
    setRandomSeed(1234);
    bool sawMin = false;
    bool sawMax = false;
    for (int i = 0; i < 2000; ++i) {
        const int roll = randomInt(1, 6);
        REQUIRE(roll >= 1);
        REQUIRE(roll <= 6);
        sawMin = sawMin || roll == 1;
        sawMax = sawMax || roll == 6;
    }
    CHECK(sawMin);
    CHECK(sawMax);

    SUBCASE("min and max the wrong way round still works") {
        std::set<int> seen;
        for (int i = 0; i < 500; ++i) {
            const int value = randomInt(3, -3);
            REQUIRE(value >= -3);
            REQUIRE(value <= 3);
            seen.insert(value);
        }
        CHECK(seen.size() == 7);
    }
    SUBCASE("a range of one number always gives that number") {
        for (int i = 0; i < 20; ++i) CHECK(randomInt(5, 5) == 5);
    }
}

TEST_CASE("randomFloat stays inside the range") {
    setRandomSeed(42);
    for (int i = 0; i < 2000; ++i) {
        const float value = randomFloat(-2.5f, 7.5f);
        REQUIRE(value >= -2.5f);
        REQUIRE(value <= 7.5f);
    }
    SUBCASE("swapped min and max") {
        for (int i = 0; i < 200; ++i) {
            const float value = randomFloat(10.0f, 0.0f);
            REQUIRE(value >= 0.0f);
            REQUIRE(value <= 10.0f);
        }
    }
    SUBCASE("min == max") {
        CHECK(randomFloat(3.0f, 3.0f) == 3.0f);
    }
    SUBCASE("values are spread out, not all the same") {
        float smallest = 100.0f;
        float biggest = -100.0f;
        for (int i = 0; i < 1000; ++i) {
            const float value = randomFloat(0.0f, 100.0f);
            smallest = std::min(smallest, value);
            biggest = std::max(biggest, value);
        }
        CHECK(smallest < 5.0f);
        CHECK(biggest > 95.0f);
    }
}

TEST_CASE("chance gives true with the given probability") {
    setRandomSeed(7);
    SUBCASE("chance(0) is never true and chance(1) is always true") {
        for (int i = 0; i < 1000; ++i) {
            REQUIRE_FALSE(chance(0.0f));
            REQUIRE(chance(1.0f));
        }
        CHECK_FALSE(chance(-3.0f));
        CHECK(chance(3.0f));
    }
    SUBCASE("chance(0.25) is true roughly a quarter of the time") {
        int hits = 0;
        for (int i = 0; i < 10000; ++i)
            if (chance(0.25f)) ++hits;
        CHECK(hits > 2200);
        CHECK(hits < 2800);
    }
}

TEST_CASE("setRandomSeed makes the random numbers repeat") {
    auto sequence = [] {
        std::vector<float> values;
        for (int i = 0; i < 10; ++i) {
            values.push_back(static_cast<float>(randomInt(0, 1000)));
            values.push_back(randomFloat(0.0f, 1.0f));
        }
        const Vec2 dir = randomDirection();
        values.push_back(dir.x);
        values.push_back(dir.y);
        return values;
    };
    setRandomSeed(99);
    const std::vector<float> first = sequence();
    setRandomSeed(99);
    const std::vector<float> second = sequence();
    CHECK(first == second);

    setRandomSeed(100);
    const std::vector<float> different = sequence();
    CHECK(first != different);
}

TEST_CASE("randomChoice picks an item from a list") {
    setRandomSeed(5);
    std::vector<std::string> names{"Ann", "Bob", "Cat"};
    std::set<std::string> picked;
    for (int i = 0; i < 300; ++i) {
        const std::string& name = randomChoice(names);
        REQUIRE((name == "Ann" || name == "Bob" || name == "Cat"));
        picked.insert(name);
    }
    CHECK(picked.size() == 3);

    SUBCASE("it returns a reference into the list, so you can change the item") {
        std::vector<int> one{10};
        randomChoice(one) = 20;
        CHECK(one[0] == 20);
    }
    SUBCASE("works with a const list") {
        const std::vector<int> numbers{4, 8};
        const int value = randomChoice(numbers);
        CHECK((value == 4 || value == 8));
    }
    SUBCASE("an empty list throws instead of crashing") {
        std::vector<int> empty;
        CHECK_THROWS_AS(randomChoice(empty), std::out_of_range);
        const std::vector<int> constEmpty;
        CHECK_THROWS_AS(randomChoice(constEmpty), std::out_of_range);
    }
}

TEST_CASE("randomDirection always has length 1") {
    setRandomSeed(11);
    bool left = false, right = false, up = false, down = false;
    for (int i = 0; i < 500; ++i) {
        const Vec2 dir = randomDirection();
        REQUIRE(dir.length() == Approx(1.0f));
        left = left || dir.x < -0.5f;
        right = right || dir.x > 0.5f;
        up = up || dir.y < -0.5f;
        down = down || dir.y > 0.5f;
    }
    CHECK((left && right && up && down)); // points every way, not just one
}

TEST_CASE("randomPointIn gives points inside the rectangle") {
    setRandomSeed(3);
    const Rect area{100, 200, 50, 10};
    for (int i = 0; i < 1000; ++i) {
        const Vec2 p = randomPointIn(area);
        REQUIRE(p.x >= area.left());
        REQUIRE(p.x <= area.right());
        REQUIRE(p.y >= area.top());
        REQUIRE(p.y <= area.bottom());
    }
    SUBCASE("a zero-size rectangle gives its corner") {
        CHECK(randomPointIn(Rect{5, 6, 0, 0}) == Vec2{5, 6});
    }
}

TEST_CASE("randomColor gives solid, bright colors") {
    setRandomSeed(8);
    for (int i = 0; i < 200; ++i) {
        const Color c = randomColor();
        REQUIRE(c.a == 255);
        // value is 1 in HSV, so the brightest channel is always full.
        REQUIRE(std::max({c.r, c.g, c.b}) == 255);
    }
}
