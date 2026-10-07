// A player in the multiplayer test: the peer that owns it moves it and counts
// up its score; the MultiplayerSynchronizer copies both to everyone else.
#include "script.hpp"

using namespace s2d;

class TestMover : public Script {
public:
    EXPORT(int, score, 0);

    void process(float delta) override {
        if (!isMultiplayerAuthority()) return;  // someone else's player: the synchronizer moves it
        self<Node2D>()->position.x += 60 * delta;
        ++score;
    }
};

REGISTER_SCRIPT(TestMover)
