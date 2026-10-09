// The autoload test (scripts/test-autoload.sh, also run by CI): GameState is
// an autoload, kept across the change from level1 to level2. Every check
// reports to it, and it says "AUTOLOAD TEST PASSED" when all of them have.
// (In a header, so the other scripts can use it: autoload<GameState>(...).)
#pragma once

#include <set>
#include <string>

#include "script.hpp"

using namespace s2d;

class GameState : public Script {
public:
    EXPORT(int, coins, 0);
    SIGNAL(coins_changed, int);
    bool isReady = false;

    void ready() override {
        isReady = true;
        // The scene is there already, but its scripts haven't started yet
        Node* marker = findNode("Level1Marker");
        if (marker && marker->script == nullptr) check("autoload_ready_before_scene");
    }

    void addCoin() {
        ++coins;
        emitSignal("coins_changed", coins);
    }

    void check(const std::string& name) {
        if (!passed.insert(name).second) return;
        print("CHECK " + name + " OK");
        for (const char* n : {"autoload_ready_before_scene", "scene_sees_ready_autoload", "signal_from_autoload",
                              "kept_across_scenes", "find_node_finds_autoload", "scene_autoload",
                              "autoload_finds_scene", "old_listener_gone", "missing_autoload_is_null"})
            if (!passed.count(n)) return;
        print("AUTOLOAD TEST PASSED");
    }

private:
    std::set<std::string> passed;
};
