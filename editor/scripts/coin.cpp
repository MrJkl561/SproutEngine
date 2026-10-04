// A coin to collect: put it on an Area2D with a CollisionShape2D child. When
// the node called "Player" touches it, the coin disappears and the Label
// called "Score" counts up.
#include "script.hpp"

using namespace s2d;

namespace {
int coinsCollected = 0;  // shared by every coin
}

class Coin : public Script {
public:
    void ready() override { coinsCollected = 0; }  // a new game has started

    void process(float) override {
        auto* area = self<Area2D>();
        if (!area) return;
        for (Node2D* body : area->getOverlappingBodies()) {
            if (body->name != "Player") continue;
            ++coinsCollected;
            if (auto* score = findNode<Label>("Score")) score->text = "Coins: " + std::to_string(coinsCollected);
            print("Picked up " + node()->name);
            area->queueFree();
            return;
        }
    }
};

REGISTER_SCRIPT(Coin)
