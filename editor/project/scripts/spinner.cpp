// Spins a Node2D (and everything under it) and makes it bob up and down.
#include <cmath>

#include "script.hpp"

using namespace s2d;

class Spinner : public Script {
public:
    EXPORT(float, degreesPerSecond, 90);
    EXPORT_STEP(float, bobHeight, 6, 0.1f);

    void ready() override {
        if (auto* me = self<Node2D>()) startY_ = me->position.y;
    }

    void process(float delta) override {
        auto* me = self<Node2D>();
        if (!me) return;
        time_ += delta;
        me->rotation += degreesPerSecond * delta;
        me->position.y = startY_ + std::sin(time_ * 3) * bobHeight;
    }

private:
    float startY_ = 0;
    float time_ = 0;
};

REGISTER_SCRIPT(Spinner)
