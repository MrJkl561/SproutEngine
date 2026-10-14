#include "script.hpp"

using namespace s2d;

// Falls; Faller lands on the solid tiles, Ghost drops through a tile that isn't solid.
class Faller : public Script {
public:
    int frames = 0;

    void process(float delta) override {
        auto* body = self<CharacterBody2D>();
        body->velocity.y += 900 * delta;
        body->moveAndSlide();
        if (++frames == 60)
            print("BODY " + body->name + " floor=" + (body->isOnFloor() ? "yes" : "no") +
                  " y=" + std::to_string(static_cast<int>(body->position.y)));
    }
};

REGISTER_SCRIPT(Faller)
