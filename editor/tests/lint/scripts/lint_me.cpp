// The script checks test (scripts/test-lint.sh): the lines marked "warn" must
// get a warning, and nothing else may.
#include "script.hpp"
using namespace s2d;

class LintMe : public Script {
public:
    EXPORT(float, speed, 200);
    AnimatedSprite2D* f = findNode<AnimatedSprite2D>("yrd");     // warn
    Node* g{getNode("Child")};                                    // warn
    Node2D* h = nullptr;                                          // fine
    std::function<void()> later = [this] { findNode("x"); };      // fine: runs later
    // AnimatedSprite2D* commented = findNode<AnimatedSprite2D>("c");  fine: comment
    const char* text = "findNode(\"in a string\")";                // fine: string

    LintMe() : h(self<Node2D>()) {                                // warn (initializer list)
        Node* inCtor = findNode("y");                            // warn (constructor)
        (void)inCtor;
    }

    void ready() override {
        h = self<Node2D>();                                       // fine
        f = findNode<AnimatedSprite2D>("yrd");                    // fine
        if (h) { getNode("a"); }                                  // fine
    }

    struct Inner {
        int x = 1;
    };
    template <class T> T* typed() { return findNode<T>("t"); }    // fine: a function
};

REGISTER_SCRIPT(LintMe)
