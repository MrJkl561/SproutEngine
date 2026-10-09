#include "game_state.hpp"

class Level1 : public Script {
public:
    void ready() override {
        GameState* gs = autoload<GameState>("GameState");
        if (gs && gs->isReady) gs->check("scene_sees_ready_autoload");
        connect(autoloadNode("GameState"), "coins_changed", [this](int n) {
            if (n == 3) autoload<GameState>("GameState")->check("signal_from_autoload");
            if (n > 3) print("FAILED: a freed scene's listener was called");
        });
        if (autoload<GameState>("NoSuchThing") == nullptr && gs) gs->check("missing_autoload_is_null");
    }

    void process(float) override {
        if (++frames == 5) {
            for (int i = 0; i < 3; ++i) autoload<GameState>("GameState")->addCoin();
            changeScene("scenes/level2.scene.json");
        }
    }

private:
    int frames = 0;
};

REGISTER_SCRIPT(Level1)
