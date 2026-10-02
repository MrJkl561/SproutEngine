// Tests for sprout/save.hpp: SaveFile, loadJson and saveJson.
//
// Every test works in its own fresh temporary folder, which is deleted afterwards.
#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include <sprout/save.hpp>

#include "test_helpers.hpp"

using namespace sprout;
using sprout_test::CaptureStderr;
using sprout_test::TempDir;

TEST_CASE("a SaveFile that doesn't exist yet starts empty") {
    TempDir dir;
    const std::string path = dir.file("save.json");
    SaveFile save{path};
    CHECK(save.path() == path);
    CHECK(save.data().is_object());
    CHECK(save.data().empty());
    CHECK_FALSE(save.has("highscore"));
    CHECK(save.get("highscore", 0) == 0);
    CHECK_FALSE(std::filesystem::exists(path)); // just opening it doesn't create the file
}

TEST_CASE("set() writes the file straight away") {
    TempDir dir;
    const std::string path = dir.file("save.json");
    {
        SaveFile save{path};
        save.set("highscore", 1200);
        CHECK(std::filesystem::exists(path));
        save.set("playerName", "Sam");
        save.set("volume", 0.75f);
        save.set("fullscreen", true);
        save.set("unlocked", std::vector<int>{1, 2, 3});
    }
    // A completely new SaveFile (like the next time the game starts) reads it all back.
    SaveFile again{path};
    CHECK(again.get("highscore", 0) == 1200);
    CHECK(again.get("playerName", "nobody") == "Sam");
    CHECK(again.get("volume", 1.0f) == doctest::Approx(0.75f));
    CHECK(again.get("fullscreen", false) == true);
    CHECK(again.get("unlocked", std::vector<int>{}) == std::vector<int>{1, 2, 3});

    SUBCASE("the file is readable JSON") {
        const Json json = loadJson(path);
        CHECK(json["highscore"] == 1200);
        CHECK(json["playerName"] == "Sam");
    }
    SUBCASE("set() replaces an old value") {
        again.set("highscore", 5000);
        CHECK(SaveFile{path}.get("highscore", 0) == 5000);
    }
}

TEST_CASE("get() gives the default for a missing key or the wrong type") {
    TempDir dir;
    SaveFile save{dir.file("save.json")};
    save.set("name", "Sam");
    save.set("score", 10);
    save.set("alive", true);

    CHECK(save.get("missing", 42) == 42);
    CHECK(save.get("missing", std::string("x")) == "x");
    CHECK(save.get("name", 0) == 0);          // a string isn't an int
    CHECK(save.get("score", "none") == "none"); // an int isn't a string
    CHECK(save.get("score", false) == false);  // an int isn't a bool
    CHECK(save.get("name", std::vector<int>{9}) == std::vector<int>{9});

    SUBCASE("numbers convert between int and float") {
        CHECK(save.get("score", 0.0f) == doctest::Approx(10.0f));
    }
}

TEST_CASE("get(key, \"text\") gives back a std::string") {
    TempDir dir;
    SaveFile save{dir.file("save.json")};
    static_assert(std::is_same_v<decltype(save.get("name", "nobody")), std::string>);
    static_assert(std::is_same_v<decltype(save.get("score", 0)), int>);
    static_assert(std::is_same_v<decltype(save.get("speed", 1.5f)), float>);
    CHECK(save.get("name", "nobody") == "nobody");
    save.set("name", "Alex");
    const std::string name = save.get("name", "nobody");
    CHECK(name == "Alex");
}

TEST_CASE("has(), remove() and clear()") {
    TempDir dir;
    const std::string path = dir.file("save.json");
    SaveFile save{path};
    save.set("a", 1);
    save.set("b", 2);
    CHECK(save.has("a"));
    CHECK(save.has("b"));
    CHECK_FALSE(save.has("c"));

    save.remove("a");
    CHECK_FALSE(save.has("a"));
    CHECK_FALSE(SaveFile{path}.has("a")); // removing also writes the file
    CHECK(SaveFile{path}.has("b"));
    CHECK_NOTHROW(save.remove("never-there"));

    save.clear();
    CHECK_FALSE(save.has("b"));
    CHECK(save.data().empty());
    CHECK(SaveFile{path}.data().empty()); // clearing also writes the file
}

TEST_CASE("a broken save file starts fresh instead of crashing") {
    TempDir dir;
    const std::string path = dir.file("save.json");

    SUBCASE("not JSON at all") {
        sprout_test::writeFile(path, "this is { not json !!");
    }
    SUBCASE("cut off half way (e.g. the computer crashed while saving)") {
        sprout_test::writeFile(path, "{ \"highscore\": 12");
    }
    SUBCASE("an empty file") {
        sprout_test::writeFile(path, "");
    }
    SUBCASE("JSON, but a list instead of an object") {
        sprout_test::writeFile(path, "[1, 2, 3]");
    }
    SUBCASE("JSON, but just a number") {
        sprout_test::writeFile(path, "42");
    }

    CaptureStderr captured;
    std::optional<SaveFile> save;
    CHECK_NOTHROW(save.emplace(path));
    REQUIRE(save.has_value());
    CHECK(save->data().is_object());
    CHECK(save->data().empty());
    CHECK(save->get("highscore", 0) == 0);
    CHECK(captured.text().find("save.json") != std::string::npos); // a warning explains what happened
    // Saving again repairs the file.
    save->set("highscore", 1);
    CHECK(SaveFile{path}.get("highscore", 0) == 1);
}

TEST_CASE("reload() reads changes made to the file by someone else") {
    TempDir dir;
    const std::string path = dir.file("save.json");
    SaveFile save{path};
    save.set("coins", 3);
    saveJson(path, Json{{"coins", 99}});
    CHECK(save.get("coins", 0) == 3);
    save.reload();
    CHECK(save.get("coins", 0) == 99);
}

TEST_CASE("data() gives the whole save as JSON") {
    TempDir dir;
    const std::string path = dir.file("save.json");
    SaveFile save{path};
    save.data()["settings"]["music"] = 0.5;
    CHECK(save.write());
    const Json json = loadJson(path);
    CHECK(json["settings"]["music"] == 0.5);
}

TEST_CASE("a save file in a folder that doesn't exist can't be written, but doesn't crash") {
    TempDir dir;
    const std::string path = dir.file("no/such/folder/save.json");
    CaptureStderr captured;
    SaveFile save{path};
    CHECK_NOTHROW(save.set("a", 1));
    CHECK_FALSE(save.write());
    CHECK(save.get("a", 0) == 1); // still remembered while the game runs
    CHECK(captured.text().find("couldn't write") != std::string::npos);
}

// Bad text must never cost the player their save: no exception, and the old data stays.
TEST_CASE("saving text that isn't valid UTF-8 doesn't throw or wipe the save file") {
    // e.g. a name typed in an old Windows code page: "Ren\xE9" is "René" in Latin-1.
    TempDir dir;
    const std::string path = dir.file("save.json");
    SaveFile save{path};
    save.set("highscore", 1200);

    CaptureStderr captured;
    CHECK_NOTHROW(save.set("playerName", std::string("Ren\xE9")));
    CHECK(SaveFile{path}.get("highscore", 0) == 1200); // the old data is still in the file
    CHECK_NOTHROW(saveJson(dir.file("other.json"), Json{{"name", std::string("\xFF\xFE")}}));
    CHECK(loadJson(dir.file("other.json")).is_object()); // still a readable JSON file
}

TEST_CASE("saving leaves no temporary files behind") {
    TempDir dir;
    const std::string path = dir.file("save.json");
    SaveFile save{path};
    save.set("a", 1);
    save.set("b", "two");
    save.remove("a");
    CHECK(saveJson(dir.file("level.json"), Json{{"x", 1}}));
    std::vector<std::string> files;
    for (const auto& entry : std::filesystem::directory_iterator(dir.path()))
        files.push_back(entry.path().filename().string());
    std::sort(files.begin(), files.end());
    CHECK(files == std::vector<std::string>{"level.json", "save.json"});
}

TEST_CASE("loadJson and saveJson") {
    TempDir dir;

    SUBCASE("round trip") {
        const Json level = {
            {"name", "Level 1"},
            {"enemies", {{{"type", "goblin"}, {"health", 3}}, {{"type", "bat"}, {"health", 1}}}},
            {"gravity", 980.5},
            {"secret", false},
        };
        const std::string path = dir.file("level.json");
        CHECK(saveJson(path, level));
        const Json loaded = loadJson(path);
        CHECK(loaded == level);
        CHECK(loaded["enemies"][0]["health"] == 3);
        CHECK(loaded["enemies"][1]["type"] == "bat");
    }
    SUBCASE("the file is nicely indented, so people can read and edit it") {
        const std::string path = dir.file("pretty.json");
        saveJson(path, Json{{"a", 1}});
        CHECK(sprout_test::readFile(path) == "{\n    \"a\": 1\n}\n");
    }
    SUBCASE("a file saved with a UTF-8 \"BOM\" (some Windows editors add one) loads fine") {
        const std::string path = dir.file("bom.json");
        sprout_test::writeFile(path, "\xEF\xBB\xBF{ \"a\": 1 }");
        CHECK(loadJson(path) == Json{{"a", 1}});
    }
    SUBCASE("a missing file gives null and a warning") {
        CaptureStderr captured;
        const Json missing = loadJson(dir.file("nope.json"));
        CHECK(missing.is_null());
        CHECK(captured.text().find("nope.json") != std::string::npos);
    }
    SUBCASE("a broken file gives null and a warning") {
        const std::string path = dir.file("broken.json");
        sprout_test::writeFile(path, "{ \"a\": [1, 2, }");
        CaptureStderr captured;
        const Json broken = loadJson(path);
        CHECK(broken.is_null());
        CHECK(captured.text().find("broken.json") != std::string::npos);
    }
    SUBCASE("saveJson into a missing folder returns false") {
        CaptureStderr captured;
        CHECK_FALSE(saveJson(dir.file("missing/folder/x.json"), Json{{"a", 1}}));
    }
}
