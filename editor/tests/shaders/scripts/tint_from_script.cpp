#include "script.hpp"

using namespace s2d;

// Sets a shader parameter from a script
class TintFromScript : public Script {
public:
    void ready() override { self<Sprite2D>()->setShaderParam("tint", Color{0, 255, 0, 255}); }
};

REGISTER_SCRIPT(TintFromScript)
