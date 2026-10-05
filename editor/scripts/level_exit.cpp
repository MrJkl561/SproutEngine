// A door to the next level: put it on an Area2D with a CollisionShape2D child.
// When the node called "Player" walks into it, the game changes to the next
// scene in the list below (and after the last one, back to the first).
#include "script.hpp"

using namespace s2d;

class LevelExit : public Script {
public:
    // Your levels, in order.
    std::vector<std::string> levels = {
        "scenes/main.scene.json",
        "scenes/level2.scene.json",
    };

    void process(float) override {
        auto* area = self<Area2D>();
        if (!area || used_) return;
        for (Node2D* body : area->getOverlappingBodies()) {
            if (body->name != "Player") continue;
            used_ = true;  // only once, even if the player stays in the door this frame
            changeScene(nextLevel());
            return;
        }
    }

private:
    std::string nextLevel() const {
        std::string current = tree() ? tree()->currentScene() : "";
        for (size_t i = 0; i < levels.size(); ++i)
            if (levels[i] == current) return levels[(i + 1) % levels.size()];
        return levels.front();
    }

    bool used_ = false;
};

REGISTER_SCRIPT(LevelExit)
