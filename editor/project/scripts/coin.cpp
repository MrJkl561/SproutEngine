// A coin to collect: put it on an Area2D with a CollisionShape2D child. When
// the node called "Player" touches it, the coin disappears and the Label
// called "Score" counts up. The count is kept in tree()->values, so it carries
// on from level to level.
//
// It listens to the Area2D's "body_entered" signal instead of checking every
// frame: the area calls onBodyEntered when something comes in.
#include "script.hpp"

using namespace s2d;

class Coin : public Script {
public:
    SIGNAL(collected);  // sent when the player picks it up (for anyone who wants to know)

    void ready() override {
        showScore();  // a new level: show the count so far
        connect(node(), "body_entered", &Coin::onBodyEntered);
    }

    void onBodyEntered(Node* body) {
        if (body->name != "Player" || node()->isQueuedForDeletion()) return;
        tree()->values["coins"] += 1;
        showScore();
        print("Picked up " + node()->name);
        emitSignal("collected");
        node()->queueFree();
    }

private:
    void showScore() {
        if (auto* score = findNode<Label>("Score"))
            score->text = "Coins: " + std::to_string(static_cast<int>(tree()->values["coins"]));
    }
};

REGISTER_SCRIPT(Coin)
