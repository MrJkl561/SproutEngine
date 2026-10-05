// A door to another scene: put it on an Area2D with a CollisionShape2D child.
// When the node called "Player" walks into it, the game changes to the scene
// picked in the inspector ("next scene", under Script Variables).
#include "script.hpp"

using namespace s2d;

class LevelExit : public Script {
public:
    std::string nextScene;  // set per door in the inspector

    void exports(Exports& e) override { e.scene("next_scene", nextScene); }

    void process(float) override {
        auto* area = self<Area2D>();
        if (!area || used_) return;
        for (Node2D* body : area->getOverlappingBodies()) {
            if (body->name != "Player") continue;
            used_ = true;  // only once, even if the player stays in the door
            if (nextScene.empty()) print("'" + node()->name + "' has no next scene set in the inspector.");
            else changeScene(nextScene);
            return;
        }
    }

private:
    bool used_ = false;
};

REGISTER_SCRIPT(LevelExit)
