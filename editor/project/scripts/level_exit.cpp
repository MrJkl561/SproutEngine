// A door to another scene: put it on an Area2D with a CollisionShape2D child.
// When the node called "Player" walks into it (the area's "body_entered"
// signal), the game changes to the scene picked in the inspector ("next
// scene", under Script Variables).
#include "script.hpp"

using namespace s2d;

class LevelExit : public Script {
public:
    EXPORT_SCENE(nextScene);  // set per door in the inspector

    void ready() override {
        // (The scene changes at the end of the frame, so this only happens once.)
        connect(node(), "body_entered", [this](Node* body) {
            if (body->name != "Player") return;
            if (nextScene.empty()) print("'" + node()->name + "' has no next scene set in the inspector.");
            else changeScene(nextScene);
        });
    }
};

REGISTER_SCRIPT(LevelExit)
