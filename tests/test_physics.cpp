// Tests for sprout/physics.hpp: moveAndCollide with rectangles and with tile maps.
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include <sprout/physics.hpp>
#include <sprout/tilemap.hpp>

#include "test_helpers.hpp"

using namespace sprout;
using doctest::Approx;

namespace {
constexpr float kFrame = 1.0f / 60.0f;

/// A floor made of separate 32x32 tiles side by side: lots of "seams" to get stuck on.
std::vector<Rect> tileFloor(float top, float firstX, int tiles) {
    std::vector<Rect> floor;
    for (int i = 0; i < tiles; ++i) floor.push_back(Rect{firstX + static_cast<float>(i) * 32.0f, top, 32, 32});
    return floor;
}

/// A wall made of 32x32 tiles stacked on top of each other.
std::vector<Rect> tileWall(float left, float firstY, int tiles) {
    std::vector<Rect> wall;
    for (int i = 0; i < tiles; ++i) wall.push_back(Rect{left, firstY + static_cast<float>(i) * 32.0f, 32, 32});
    return wall;
}
} // namespace

TEST_CASE("MoveResult helpers") {
    MoveResult none;
    CHECK_FALSE(none.hitAnything());
    CHECK_FALSE(none.hitWall());

    MoveResult left;
    left.hitLeft = true;
    CHECK(left.hitAnything());
    CHECK(left.hitWall());

    MoveResult floor;
    floor.hitBottom = true;
    CHECK(floor.hitAnything());
    CHECK_FALSE(floor.hitWall());
}

TEST_CASE("moveAndCollide with nothing in the way moves the whole distance") {
    Rect box{0, 0, 10, 10};
    const MoveResult result = moveAndCollide(box, Vec2{100, 50}, std::vector<Rect>{});
    CHECK(box == Rect{100, 50, 10, 10});
    CHECK_FALSE(result.hitAnything());

    SUBCASE("solids that are far away don't matter") {
        Rect other{0, 0, 10, 10};
        const MoveResult r = moveAndCollide(other, Vec2{-30, 20}, std::vector<Rect>{{500, 500, 10, 10}});
        CHECK(other == Rect{-30, 20, 10, 10});
        CHECK_FALSE(r.hitAnything());
    }
    SUBCASE("moving by zero does nothing") {
        Rect still{5, 5, 10, 10};
        const MoveResult r = moveAndCollide(still, Vec2{0, 0}, std::vector<Rect>{{0, 15, 100, 10}});
        CHECK(still == Rect{5, 5, 10, 10});
        CHECK_FALSE(r.hitAnything());
    }
}

TEST_CASE("moveAndCollide: landing on a floor") {
    const std::vector<Rect> solids{{0, 100, 200, 20}};
    Rect box{50, 50, 20, 20};
    const MoveResult result = moveAndCollide(box, Vec2{0, 100}, solids);
    CHECK(result.hitBottom);
    CHECK_FALSE(result.hitTop);
    CHECK_FALSE(result.hitWall());
    CHECK(box.bottom() == Approx(100.0f)); // resting exactly on top of the floor
    CHECK(box.x == 50.0f);

    SUBCASE("standing still on the floor with gravity keeps it on the floor") {
        for (int frame = 0; frame < 120; ++frame) {
            const MoveResult r = moveAndCollide(box, Vec2{0, 10}, solids);
            REQUIRE(r.hitBottom);
        }
        CHECK(box.bottom() == Approx(100.0f));
    }
}

TEST_CASE("moveAndCollide: hitting walls on the right and on the left") {
    SUBCASE("wall on the right") {
        const std::vector<Rect> solids{{100, 0, 20, 200}};
        Rect box{50, 50, 20, 20};
        const MoveResult result = moveAndCollide(box, Vec2{100, 0}, solids);
        CHECK(result.hitRight);
        CHECK_FALSE(result.hitLeft);
        CHECK_FALSE(result.hitBottom);
        CHECK(box.right() == Approx(100.0f));
        CHECK(box.y == 50.0f);
    }
    SUBCASE("wall on the left") {
        const std::vector<Rect> solids{{0, 0, 20, 200}};
        Rect box{50, 50, 20, 20};
        const MoveResult result = moveAndCollide(box, Vec2{-100, 0}, solids);
        CHECK(result.hitLeft);
        CHECK_FALSE(result.hitRight);
        CHECK(box.left() == Approx(20.0f));
    }
    SUBCASE("pushing against a wall you already touch still reports the wall") {
        const std::vector<Rect> solids{{100, 0, 20, 200}};
        Rect box{80, 50, 20, 20};
        const MoveResult result = moveAndCollide(box, Vec2{5, 0}, solids);
        CHECK(result.hitRight);
        CHECK(box.right() == Approx(100.0f));
    }
}

TEST_CASE("moveAndCollide: bumping into a ceiling") {
    const std::vector<Rect> solids{{0, 0, 200, 20}};
    Rect box{50, 50, 20, 20};
    const MoveResult result = moveAndCollide(box, Vec2{0, -100}, solids);
    CHECK(result.hitTop);
    CHECK_FALSE(result.hitBottom);
    CHECK(box.top() == Approx(20.0f));
}

TEST_CASE("moveAndCollide slides along walls instead of stopping") {
    SUBCASE("falling diagonally onto a floor keeps moving sideways") {
        const std::vector<Rect> solids{{0, 100, 400, 20}};
        Rect box{50, 70, 20, 20};
        const MoveResult result = moveAndCollide(box, Vec2{50, 50}, solids);
        CHECK(result.hitBottom);
        CHECK_FALSE(result.hitWall());
        CHECK(box.x == Approx(100.0f));
        CHECK(box.bottom() == Approx(100.0f));
    }
    SUBCASE("moving diagonally into a wall keeps moving up/down") {
        const std::vector<Rect> solids{{100, 0, 20, 400}};
        Rect box{70, 50, 20, 20};
        const MoveResult result = moveAndCollide(box, Vec2{50, 60}, solids);
        CHECK(result.hitRight);
        CHECK_FALSE(result.hitBottom);
        CHECK(box.right() == Approx(100.0f));
        CHECK(box.y == Approx(110.0f));
    }
    SUBCASE("into a corner: stops on both axes") {
        const std::vector<Rect> solids{{0, 100, 200, 20}, {100, 0, 20, 100}};
        Rect box{60, 60, 20, 20};
        const MoveResult result = moveAndCollide(box, Vec2{100, 100}, solids);
        CHECK(result.hitRight);
        CHECK(result.hitBottom);
        CHECK(box.right() == Approx(100.0f));
        CHECK(box.bottom() == Approx(100.0f));
    }
}

TEST_CASE("moveAndCollide: sliding along a floor made of tiles doesn't get stuck on the seams") {
    // A box with an "awkward" height resting on a row of separate tiles, then
    // walking right across many of them with gravity pulling it down every frame.
    // Rounding errors must not make the box bump into the side of the next tile.
    const float height = 28.7f;
    const std::vector<Rect> floor = tileFloor(320.0f, 0.0f, 40);
    Rect box{10.3f, 320.0f - height, 20.0f, height};

    for (int frame = 0; frame < 180; ++frame) { // 3 seconds at 200 pixels per second
        CAPTURE(frame);
        const float before = box.x;
        const MoveResult result = moveAndCollide(box, Vec2{200, 600} * kFrame, floor);
        REQUIRE_FALSE(result.hitWall());
        REQUIRE(result.hitBottom);
        REQUIRE(box.x - before == Approx(200.0f * kFrame));
        REQUIRE(box.bottom() == Approx(320.0f));
    }
    CHECK(box.x == Approx(10.3f + 600.0f).epsilon(0.001));

    SUBCASE("...and back to the left") {
        for (int frame = 0; frame < 120; ++frame) {
            const MoveResult result = moveAndCollide(box, Vec2{-200, 600} * kFrame, floor);
            REQUIRE_FALSE(result.hitWall());
        }
    }
    SUBCASE("...without gravity too") {
        for (int frame = 0; frame < 120; ++frame) {
            const MoveResult result = moveAndCollide(box, Vec2{-150, 0} * kFrame, floor);
            REQUIRE_FALSE(result.hitAnything());
        }
    }
}

TEST_CASE("moveAndCollide: falling along a wall made of tiles doesn't get stuck on the seams") {
    const float width = 20.3f;
    const std::vector<Rect> wall = tileWall(500.0f, 0.0f, 40);
    Rect box{500.0f - width, 10.0f, width, 30.0f};
    for (int frame = 0; frame < 120; ++frame) {
        CAPTURE(frame);
        const float before = box.y;
        const MoveResult result = moveAndCollide(box, Vec2{100, 300} * kFrame, wall); // pushing into the wall
        REQUIRE(result.hitRight);
        REQUIRE_FALSE(result.hitBottom);
        REQUIRE(box.y - before == Approx(300.0f * kFrame));
    }
}

TEST_CASE("moveAndCollide: no seam problems far away from the origin either") {
    // Floats get less precise for big numbers, e.g. at the end of a long level.
    for (const float far : {5000.0f, 20000.0f, 70000.0f}) {
        CAPTURE(far);
        const std::vector<Rect> floor = tileFloor(far, far, 30);
        Rect box{far + 10.3f, far - 28.7f, 20.0f, 28.7f};
        for (int frame = 0; frame < 120; ++frame) {
            const MoveResult result = moveAndCollide(box, Vec2{200, 600} * kFrame, floor);
            REQUIRE_FALSE(result.hitWall());
            REQUIRE(result.hitBottom);
        }
        CHECK(box.x == Approx(far + 10.3f + 400.0f).epsilon(0.0001));
    }
}

TEST_CASE("moveAndCollide: very fast boxes don't tunnel through thin walls") {
    const std::vector<Rect> solids{{300, 0, 2, 600}}; // a wall only 2 pixels thick

    SUBCASE("horizontally") {
        Rect box{0, 100, 20, 20};
        const MoveResult result = moveAndCollide(box, Vec2{10000, 0}, solids);
        CHECK(result.hitRight);
        CHECK(box.right() == Approx(300.0f));
    }
    SUBCASE("a small bullet") {
        Rect bullet{0, 100, 4, 4};
        const MoveResult result = moveAndCollide(bullet, Vec2{5000, 0}, solids);
        CHECK(result.hitRight);
        CHECK(bullet.right() == Approx(300.0f));
    }
    SUBCASE("vertically through a thin floor") {
        const std::vector<Rect> thinFloor{{0, 300, 600, 1}};
        Rect box{100, 0, 20, 20};
        const MoveResult result = moveAndCollide(box, Vec2{0, 50000}, thinFloor);
        CHECK(result.hitBottom);
        CHECK(box.bottom() == Approx(300.0f));
    }
    SUBCASE("diagonally") {
        Rect box{0, 0, 16, 16};
        const MoveResult result = moveAndCollide(box, Vec2{3000, 1000}, solids);
        CHECK(result.hitRight);
        CHECK(box.right() == Approx(300.0f));
    }
}

TEST_CASE("moveAndCollide with a TileMap") {
    TileMap level{32, {
        "##########",
        "#........#",
        "#........#",
        "#...##...#",
        "##########",
    }};
    level.setSolid("#");

    SUBCASE("landing on the floor") {
        Rect box{40, 40, 20, 20};
        const MoveResult result = moveAndCollide(box, Vec2{0, 500}, level);
        CHECK(result.hitBottom);
        CHECK(box.bottom() == Approx(128.0f)); // the floor (row 4) starts at y = 128
    }
    SUBCASE("landing on the block in the middle") {
        Rect box{130, 40, 20, 20};
        const MoveResult result = moveAndCollide(box, Vec2{0, 500}, level);
        CHECK(result.hitBottom);
        CHECK(box.bottom() == Approx(96.0f)); // the block (row 3) starts at y = 96
    }
    SUBCASE("walls and ceiling") {
        Rect box{100, 40, 20, 20};
        CHECK(moveAndCollide(box, Vec2{1000, 0}, level).hitRight);
        CHECK(box.right() == Approx(288.0f));
        CHECK(moveAndCollide(box, Vec2{-1000, 0}, level).hitLeft);
        CHECK(box.left() == Approx(32.0f));
        CHECK(moveAndCollide(box, Vec2{0, -1000}, level).hitTop);
        CHECK(box.top() == Approx(32.0f));
    }
    SUBCASE("walking into the side of the middle block") {
        Rect box{40, 128 - 20, 20, 20}; // standing on the floor
        const MoveResult result = moveAndCollide(box, Vec2{200, 1}, level);
        CHECK(result.hitRight);
        CHECK(result.hitBottom);
        CHECK(box.right() == Approx(128.0f));
    }
    SUBCASE("the map's position is taken into account") {
        level.position = {1000, 1000};
        Rect box{1040, 1040, 20, 20};
        const MoveResult result = moveAndCollide(box, Vec2{0, 500}, level);
        CHECK(result.hitBottom);
        CHECK(box.bottom() == Approx(1128.0f));
    }
    SUBCASE("a map without solid tiles blocks nothing") {
        TileMap open{32, {"####", "####"}};
        Rect box{10, 10, 10, 10};
        CHECK_FALSE(moveAndCollide(box, Vec2{40, 20}, open).hitAnything());
        CHECK(box == Rect{50, 30, 10, 10});
    }
    SUBCASE("outside the map nothing is solid") {
        Rect box{-500, -500, 10, 10};
        CHECK_FALSE(moveAndCollide(box, Vec2{100, 100}, level).hitAnything());
    }
}

TEST_CASE("moveAndCollide with a TileMap: sliding across many floor tiles") {
    TileMap level{32, {
        "                                        ",
        "                                        ",
        "########################################",
    }};
    level.setSolid("#");
    const float height = 28.7f;
    Rect box{5.0f, 64.0f - height, 20.0f, height};

    for (int frame = 0; frame < 300; ++frame) {
        CAPTURE(frame);
        const MoveResult result = moveAndCollide(box, Vec2{250, 800} * kFrame, level);
        REQUIRE_FALSE(result.hitWall());
        REQUIRE(result.hitBottom);
    }
    CHECK(box.x == Approx(5.0f + 1250.0f).epsilon(0.001));
    CHECK(box.bottom() == Approx(64.0f));
}

TEST_CASE("moveAndCollide with a TileMap: fast fall onto a one-tile-thick floor") {
    TileMap level{8, {"        ", "        ", "########"}};
    level.setSolid("#");
    Rect box{10, -2000, 6, 6};
    const MoveResult result = moveAndCollide(box, Vec2{0, 5000}, level);
    CHECK(result.hitBottom);
    CHECK(box.bottom() == Approx(16.0f));
}
