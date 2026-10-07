// Walks right through the Area, and tells anyone listening when it's far along.
#include "script.hpp"

using namespace s2d;

class Walker : public Script {
public:
    SIGNAL(moved_far, float);  // sends its x position

    void process(float delta) override {
        auto* me = self<Node2D>();
        me->position.x += 300 * delta;
        if (!told && me->position.x > 150) {
            told = true;
            emitSignal("moved_far", me->position.x);
        }
    }

private:
    bool told = false;
};

REGISTER_SCRIPT(Walker)
