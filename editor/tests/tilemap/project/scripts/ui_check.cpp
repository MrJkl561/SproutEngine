#include "script.hpp"

using namespace s2d;

// Counts what the UI nodes send while SPROUT_TEST_CLICKS clicks them, and tries
// the TileMap functions scripts use.
class UiCheck : public Script {
public:
    int play = 0, world = 0, toggles = 0, frames = 0;
    bool sound = false;

    void ready() override {
        connect(getNode("Hud/Menu/Play"), "pressed", [this] { ++play; });
        connect(getNode("WorldButton"), "pressed", [this] { ++world; });
        connect(getNode("Hud/Menu/Sound"), "toggled", [this](bool on) {
            ++toggles;
            sound = on;
        });

        auto* map = getNode("Ground")->as<TileMap>();
        Vector2 cell = map->worldToMap({85, 130});
        std::string before = std::to_string(map->getCell(5, 8));
        map->setCell(5, 8, 2);
        print("MAPAPI cell=" + std::to_string((int)cell.x) + "," + std::to_string((int)cell.y) + " tile=" + before +
              " now=" + std::to_string(map->getCell(5, 8)) + " solid=" + std::to_string(map->isSolid(0, 8)) +
              std::to_string(map->isSolid(12, 8)) + std::to_string(map->isSolid(5, 8)) +
              " type=" + map->getCellType(12, 8) + " empty=" + std::to_string(map->getCell(50, 50)));
        map->setCell(5, 8, 0);
        // (the same with a cell as a Vector2)
        if (map->getCellType(map->worldToMap({200, 130})) != "grass" || !map->isSolid(Vector2{3, 8}))
            print("MAPAPI the Vector2 versions are WRONG");
    }

    void process(float) override {
        ++frames;
        if (hasArg("--quit-test") && frames == 5) {  // quit() ends the game (the bodies never report)
            print("QUITTING");
            quit();
        }
        if (frames != 40) return;
        auto* menu = getNode("Hud/Menu")->as<Control>();
        auto* quit = getNode("Hud/Menu/Quit")->as<Control>();
        print("UI play=" + std::to_string(play) + " world=" + std::to_string(world) + " toggles=" +
              std::to_string(toggles) + " sound=" + (sound ? "on" : "off") + " quitY=" +
              std::to_string((int)quit->position.y) + " quitW=" + std::to_string((int)quit->size.x) +
              " menuH=" + std::to_string((int)menu->size.y));
    }
};

REGISTER_SCRIPT(UiCheck)
