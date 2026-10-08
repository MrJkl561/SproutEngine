// Counts the Tick timer's timeouts. The test frees this node part-way through:
// after that, the timer must not call it any more.
#include "script.hpp"

using namespace s2d;

class Listener : public Script {
public:
    void ready() override {
        connect(node()->parent()->getNode("Tick"), "timeout", [this] { tree()->values["listener_ticks"] += 1; });
    }
};

REGISTER_SCRIPT(Listener)
