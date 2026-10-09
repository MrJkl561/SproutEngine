#include "game_state.hpp"

// On the Hud autoload (a scene): it can see whatever scene is on.
class HudScript : public Script {
public:
    void process(float) override {
        if (findNode("Level2Marker")) autoload<GameState>("GameState")->check("autoload_finds_scene");
        if (auto* label = self<Label>()) label->text = "Coins: " + std::to_string(autoload<GameState>("GameState")->coins);
    }
};

REGISTER_SCRIPT(HudScript)
