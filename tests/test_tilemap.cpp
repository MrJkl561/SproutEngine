// Tests for sprout/tilemap.hpp (everything except draw(), which needs a window).
#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

#include <sprout/tilemap.hpp>

#include "test_helpers.hpp"

using namespace sprout;
using sprout_test::CaptureStderr;
using sprout_test::TempDir;

namespace {
TileMap makeLevel() {
    return TileMap{32, {
        "##########",
        "#........#",
        "#..P...C.#",
        "#....##..#",
        "##########",
    }};
}
} // namespace

TEST_CASE("TileMap size") {
    const TileMap level = makeLevel();
    CHECK(level.tileSize() == 32);
    CHECK(level.columns() == 10);
    CHECK(level.rows() == 5);
    CHECK(level.width() == 320.0f);
    CHECK(level.height() == 160.0f);
    CHECK(level.bounds() == Rect{0, 0, 320, 160});

    SUBCASE("bounds follow the map's position") {
        TileMap moved = makeLevel();
        moved.position = {100, 50};
        CHECK(moved.bounds() == Rect{100, 50, 320, 160});
    }
    SUBCASE("an empty map") {
        const TileMap empty;
        CHECK(empty.rows() == 0);
        CHECK(empty.columns() == 0);
        CHECK(empty.width() == 0.0f);
        CHECK(empty.getTile(0, 0) == ' ');
        CHECK(empty.findTiles('#').empty());
    }
    SUBCASE("a tile size of 0 or less is made 1 instead of dividing by zero later") {
        const TileMap tiny{0, {"##"}};
        CHECK(tiny.tileSize() == 1);
        CHECK(tiny.coordAt({1.5f, 0.5f}) == TileCoord{1, 0});
    }
}

TEST_CASE("TileMap pads short rows with empty tiles") {
    const TileMap level{16, {"#####", "#", "##"}};
    CHECK(level.columns() == 5);
    CHECK(level.rows() == 3);
    CHECK(level.getTile(0, 1) == '#');
    CHECK(level.getTile(1, 1) == ' ');
    CHECK(level.getTile(4, 1) == ' ');
    CHECK(level.getTile(1, 2) == '#');
    CHECK(level.getTile(2, 2) == ' ');
    CHECK(level.countTiles(' ') == 7);
}

TEST_CASE("TileMap getTile and setTile") {
    TileMap level = makeLevel();
    CHECK(level.getTile(0, 0) == '#');
    CHECK(level.getTile(3, 2) == 'P');
    CHECK(level.getTile(7, 2) == 'C');
    CHECK(level.getTile(1, 1) == '.');

    level.setTile(1, 1, 'X');
    CHECK(level.getTile(1, 1) == 'X');

    SUBCASE("outside the map is safe: reading gives ' ' and writing does nothing") {
        CHECK(level.getTile(-1, 0) == ' ');
        CHECK(level.getTile(0, -1) == ' ');
        CHECK(level.getTile(10, 0) == ' ');
        CHECK(level.getTile(0, 5) == ' ');
        CHECK(level.getTile(1000, 1000) == ' ');
        level.setTile(-1, -1, 'X');
        level.setTile(10, 0, 'X');
        level.setTile(0, 5, 'X');
        CHECK(level.countTiles('X') == 1);
        CHECK(level.columns() == 10);
        CHECK(level.rows() == 5);
    }
}

TEST_CASE("TileMap converts between world positions and tiles") {
    TileMap level = makeLevel();

    SUBCASE("coordAt and tileAt") {
        CHECK(level.coordAt({0, 0}) == TileCoord{0, 0});
        CHECK(level.coordAt({31.9f, 31.9f}) == TileCoord{0, 0});
        CHECK(level.coordAt({32, 32}) == TileCoord{1, 1});
        CHECK(level.coordAt({3 * 32 + 5, 2 * 32 + 5}) == TileCoord{3, 2});
        CHECK(level.tileAt({3 * 32 + 5, 2 * 32 + 5}) == 'P');
        CHECK(level.tileAt({5000, 5000}) == ' ');
    }
    SUBCASE("negative positions round DOWN (to -1), not towards zero") {
        CHECK(level.coordAt({-1, -1}) == TileCoord{-1, -1});
        CHECK(level.coordAt({-0.5f, 10}) == TileCoord{-1, 0});
        CHECK(level.coordAt({-32, -33}) == TileCoord{-1, -2});
        CHECK(level.tileAt({-1, 10}) == ' '); // not the '#' in column 0
    }
    SUBCASE("the map's position moves everything") {
        level.position = {100, 200};
        CHECK(level.coordAt({100, 200}) == TileCoord{0, 0});
        CHECK(level.coordAt({99, 200}) == TileCoord{-1, 0});
        CHECK(level.tileAt({100 + 3 * 32 + 1, 200 + 2 * 32 + 1}) == 'P');
        CHECK(level.tileRect(3, 2) == Rect{196, 264, 32, 32});
        CHECK(level.tileCenter(3, 2) == Vec2{212, 280});
    }
    SUBCASE("tileRect and tileCenter") {
        CHECK(level.tileRect(0, 0) == Rect{0, 0, 32, 32});
        CHECK(level.tileRect(3, 2) == Rect{96, 64, 32, 32});
        CHECK(level.tileCenter(3, 2) == Vec2{112, 80});
        // The center of a tile is in that tile.
        const Vec2 center = level.tileCenter(7, 2);
        CHECK(level.coordAt(center) == TileCoord{7, 2});
    }
    SUBCASE("setTileAt changes the tile under a point") {
        level.setTileAt({112, 80}, '.');
        CHECK(level.getTile(3, 2) == '.');
        level.setTileAt({-50, -50}, 'X'); // outside: ignored
        CHECK(level.countTiles('X') == 0);
    }
}

TEST_CASE("TileMap findTiles, replaceTiles and countTiles") {
    TileMap level = makeLevel();

    SUBCASE("findTiles gives the middle of every matching tile") {
        const std::vector<Vec2> players = level.findTiles('P');
        REQUIRE(players.size() == 1);
        CHECK(players[0] == Vec2{112, 80});
        CHECK(level.findTiles('Z').empty());
        CHECK(level.findTiles('#').size() == 28);
    }
    SUBCASE("findTiles goes left to right, top to bottom") {
        const TileMap coins{10, {"C.C", "..C"}};
        const std::vector<Vec2> found = coins.findTiles('C');
        REQUIRE(found.size() == 3);
        CHECK(found[0] == Vec2{5, 5});
        CHECK(found[1] == Vec2{25, 5});
        CHECK(found[2] == Vec2{25, 15});
    }
    SUBCASE("replaceTiles and countTiles") {
        CHECK(level.countTiles('#') == 28);
        CHECK(level.countTiles('.') == 20);
        level.replaceTiles('P', '.');
        CHECK(level.countTiles('P') == 0);
        CHECK(level.countTiles('.') == 21);
        level.replaceTiles('#', '=');
        CHECK(level.countTiles('#') == 0);
        CHECK(level.countTiles('=') == 28);
    }
}

TEST_CASE("TileMap solid tiles") {
    TileMap level = makeLevel();
    CHECK_FALSE(level.isSolid('#')); // nothing is solid until you say so

    level.setSolid("#");
    CHECK(level.isSolid('#'));
    CHECK_FALSE(level.isSolid('.'));
    CHECK_FALSE(level.isSolid(' '));

    SUBCASE("setSolid replaces the list, addSolid adds to it") {
        level.setSolid("=B");
        CHECK_FALSE(level.isSolid('#'));
        CHECK(level.isSolid('='));
        CHECK(level.isSolid('B'));
        level.addSolid('#');
        level.addSolid('#'); // adding twice is harmless
        CHECK(level.isSolid('#'));
        CHECK(level.isSolid('='));
    }
    SUBCASE("isSolidAt checks the tile under a world point") {
        CHECK(level.isSolidAt({5, 5}));
        CHECK_FALSE(level.isSolidAt({40, 40}));
        CHECK(level.isSolidAt({5 * 32 + 1, 3 * 32 + 1}));
        CHECK_FALSE(level.isSolidAt({-5, -5})); // outside the map is empty
        CHECK_FALSE(level.isSolidAt({10000, 5}));
    }
}

TEST_CASE("TileMap::solidRectsIn lists the solid tiles in an area") {
    TileMap level = makeLevel();

    SUBCASE("nothing when no tiles are solid") {
        CHECK(level.solidRectsIn(level.bounds()).empty());
    }

    level.setSolid("#");

    SUBCASE("the whole map") {
        CHECK(level.solidRectsIn(level.bounds()).size() == 28);
    }
    SUBCASE("an area in the open gives nothing") {
        CHECK(level.solidRectsIn(Rect{40, 40, 20, 20}).empty());
    }
    SUBCASE("an area over the two tiles in the middle") {
        const std::vector<Rect> found = level.solidRectsIn(Rect{165, 100, 40, 10});
        REQUIRE(found.size() == 2);
        CHECK(std::find(found.begin(), found.end(), Rect{160, 96, 32, 32}) != found.end());
        CHECK(std::find(found.begin(), found.end(), Rect{192, 96, 32, 32}) != found.end());
    }
    SUBCASE("every rect really touches the area") {
        const Rect area{20, 20, 50, 50};
        for (const Rect& r : level.solidRectsIn(area)) {
            CHECK(r.grown(0.01f).overlaps(area));
            CHECK(level.isSolidAt(r.center()));
        }
    }
    SUBCASE("areas partly or fully outside the map don't crash") {
        CHECK(level.solidRectsIn(Rect{-100, -100, 110, 110}).size() == 1); // just the top-left corner
        CHECK(level.solidRectsIn(Rect{-1000, -1000, 10, 10}).empty());
        CHECK(level.solidRectsIn(Rect{5000, 5000, 10, 10}).empty());
        CHECK(level.solidRectsIn(Rect{-1000, -1000, 5000, 5000}).size() == 28);
    }
    SUBCASE("the map's position is used") {
        level.position = {1000, 1000};
        CHECK(level.solidRectsIn(Rect{0, 0, 32, 32}).empty());
        const std::vector<Rect> found = level.solidRectsIn(Rect{1001, 1001, 5, 5});
        REQUIRE(found.size() == 1);
        CHECK(found[0] == Rect{1000, 1000, 32, 32});
    }
}

TEST_CASE("TileMap::fromFile loads a level from a text file") {
    TempDir dir;

    SUBCASE("normal file") {
        const std::string path = dir.file("level.txt");
        sprout_test::writeFile(path, "#####\n#.P.#\n#####\n");
        const TileMap level = TileMap::fromFile(path, 16);
        CHECK(level.tileSize() == 16);
        CHECK(level.rows() == 3);
        CHECK(level.columns() == 5);
        CHECK(level.getTile(2, 1) == 'P');
        CHECK(level.countTiles('#') == 12);
    }
    SUBCASE("Windows line endings (\\r\\n) don't end up in the map") {
        const std::string path = dir.file("windows.txt");
        sprout_test::writeFile(path, "###\r\n#P#\r\n###\r\n");
        const TileMap level = TileMap::fromFile(path, 32);
        CHECK(level.rows() == 3);
        CHECK(level.columns() == 3);
        CHECK(level.countTiles('\r') == 0);
        CHECK(level.getTile(1, 1) == 'P');
    }
    SUBCASE("no newline at the end, and rows of different lengths") {
        const std::string path = dir.file("ragged.txt");
        sprout_test::writeFile(path, "#\n###\n##");
        const TileMap level = TileMap::fromFile(path, 32);
        CHECK(level.rows() == 3);
        CHECK(level.columns() == 3);
        CHECK(level.getTile(2, 0) == ' ');
        CHECK(level.getTile(1, 2) == '#');
    }
    SUBCASE("a file saved with a UTF-8 \"BOM\" (some Windows editors add one) loads the same") {
        const std::string path = dir.file("bom.txt");
        sprout_test::writeFile(path, "\xEF\xBB\xBF###\r\n#P#\r\n###\r\n");
        const TileMap level = TileMap::fromFile(path, 32);
        CHECK(level.columns() == 3);
        CHECK(level.rows() == 3);
        CHECK(level.getTile(0, 0) == '#');
        CHECK(level.getTile(1, 1) == 'P');
    }
    SUBCASE("a missing file gives an empty map and a warning, not a crash") {
        CaptureStderr captured;
        const TileMap level = TileMap::fromFile(dir.file("does_not_exist.txt"), 32);
        CHECK(level.rows() == 0);
        CHECK(level.columns() == 0);
        CHECK(level.tileSize() == 32);
        CHECK(level.getTile(0, 0) == ' ');
        CHECK(captured.text().find("does_not_exist.txt") != std::string::npos);
    }
}
