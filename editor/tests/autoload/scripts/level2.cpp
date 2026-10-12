#include "game_state.hpp"

class Level2 : public Script {
public:
    void ready() override {
        GameState* gs = autoload<GameState>("GameState");
        if (!gs) return;
        if (gs->coins == 3) gs->check("kept_across_scenes");
        if (findNode("GameState") == autoloadNode("GameState")) gs->check("find_node_finds_autoload");
        if (autoload<Label>("Hud")) gs->check("scene_autoload");
        gs->addCoin();  // level1's listener is gone with level1: nothing must call it
        if (gs->coins == 4) gs->check("old_listener_gone");
    }
};

REGISTER_SCRIPT(Level2)
