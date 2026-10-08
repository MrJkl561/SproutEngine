// A player in the multiplayer test, moved by prediction: its player's game
// sends "move right" every tick and moves it at once; the server makes the
// real move from those inputs and sends the result (position and score)
// back to everyone through the MultiplayerSynchronizer.
#include "script.hpp"

using namespace s2d;

class TestMover : public Script {
public:
    EXPORT(int, score, 0);
    INPUT(float, moveX, 0);

    void gatherInput() override { moveX = 1; }  // always right

    void simulate(float delta) override {
        self<Node2D>()->position.x += 60 * moveX * delta;
        ++score;
    }
};

REGISTER_SCRIPT(TestMover)
