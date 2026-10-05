// A coin to collect: put it on an Area2D with a CollisionShape2D child. When
// the node called "Player" touches it, the coin disappears and the Label
// called "Score" counts up. The count is kept in tree()->values, so it carries
// on from level to level.
#include "script.hpp"

using namespace s2d;

class Coin : public Script {
public:
    void ready() override { showScore(); }  // a new level: show the count so far

    void process(float) override {
        auto* area = self<Area2D>();
        if (!area) return;
        for (Node2D* body : area->getOverlappingBodies()) {
            if (body->name != "Player") continue;
            tree()->values["coins"] += 1;
            showScore();
            print("Picked up " + node()->name);
            area->queueFree();
            return;
        }
    }

private:
    void showScore() {
        if (auto* score = findNode<Label>("Score"))
            score->text = "Coins: " + std::to_string(static_cast<int>(tree()->values["coins"]));
    }
};

REGISTER_SCRIPT(Coin)
