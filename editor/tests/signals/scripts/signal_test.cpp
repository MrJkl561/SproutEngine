// The signals test (scripts/test-signals.sh, also run by CI): connects to
// built-in and script signals in every way, prints "CHECK <name> OK" for
// each thing that worked and "SIGNALS TEST PASSED" when all of them have.
#include <set>
#include <string>
#include <vector>

#include "script.hpp"

using namespace s2d;

class SignalTest : public Script {
public:
    void ready() override {
        // A function of this script, taking the body as Node*
        connect(getNode("Area"), "body_entered", &SignalTest::onBodyEntered);
        // A lambda asking for a more exact node type
        connect(getNode("Area"), "body_exited", [this](CharacterBody2D* body) {
            if (body && body->name == "Body") check("body_exited_typed_lambda");
            if (body && body->name == "Body2") check("body_exited_when_freed");
        });
        // Timers
        connect(getNode("Tick"), "timeout", [this] { ++ticks; });
        connect(getNode("Tick"), "timeout", [this] { ++oneShotCalls; }, true);  // only once
        tickId = connect(getNode("Tick"), "timeout", &SignalTest::countUntilDisconnected);
        connect(getNode("Once"), "timeout", [this] { ++onceTimeouts; });
        // A script's own signal, with a value, and a listener that ignores the value
        connect(getNode("Body"), "moved_far", [this](float x) {
            if (x > 150) check("custom_signal_value");
        });
        connect(getNode("Body"), "moved_far", [this] { check("listener_takes_fewer_values"); });
        connect(getNode("Body"), "started", &SignalTest::onStarted);  // a signal with no values
        connect(getNode("Anim"), "animation_finished", [this] { check("animation_finished"); });
        connect(getNode("Body"), "tree_exiting", [this] { check("tree_exiting"); });
        // "ready" of a node added later
        auto late = createNode("Node");
        late->name = "Late";
        connect(late.get(), "ready", [this] { check("ready"); });
        node()->addChild(std::move(late));
        // A misspelled signal: a warning, not a crash
        connect(getNode("Tick"), "timout", [] {});
    }

    void onStarted() { check("signal_without_values"); }

    // Connected in the editor (saved in the scene file, see Tick and Area)
    CALLABLE(onTickFromEditor);
    void onTickFromEditor() { ++editorTicks; }
    CALLABLE(onTickOnce);
    void onTickOnce() { ++editorOnce; }
    CALLABLE(onAreaBody);
    void onAreaBody(Node* body) {
        if (body && body->name == "Body") check("editor_connection_with_value");
    }

    void onBodyEntered(Node* body) {
        if (body->name == "Body") check("body_entered_method");
    }
    void countUntilDisconnected() { ++disconnectable; }

    void process(float delta) override {
        double before = time;
        time += delta;
        auto at = [&](double t) { return before < t && time >= t; };
        if (at(0.3)) getNode("Listener")->queueFree();
        if (at(0.35)) {
            if (ticks >= 3) check("timer_repeats");
            if (oneShotCalls == 1) check("one_shot_connection");
            if (editorTicks >= 3) check("editor_connection");
            if (editorOnce == 1) check("editor_connection_one_shot");
            disconnect(tickId);
            disconnectableAt = disconnectable;
            listenerAt = tree()->values["listener_ticks"];
        }
        if (at(0.5)) getNode("Body2")->queueFree();
        if (at(0.65)) {
            if (disconnectable == disconnectableAt) check("disconnect");
            if (onceTimeouts == 1) check("one_shot_timer");
            if (listenerAt >= 2 && tree()->values["listener_ticks"] == listenerAt) check("freed_listener_not_called");
        }
        if (at(1.0)) getNode("Body")->queueFree();
    }

private:
    void check(const std::string& name) {
        if (!passed.insert(name).second) return;
        print("CHECK " + name + " OK");
        static const std::vector<std::string> all = {
            "body_entered_method", "body_exited_typed_lambda", "body_exited_when_freed", "timer_repeats",
            "one_shot_connection", "disconnect", "one_shot_timer", "freed_listener_not_called", "custom_signal_value",
            "listener_takes_fewer_values", "animation_finished", "tree_exiting", "ready", "editor_connection",
            "editor_connection_one_shot", "editor_connection_with_value", "signal_without_values"};
        for (const auto& n : all)
            if (!passed.count(n)) return;
        print("SIGNALS TEST PASSED");
    }

    double time = 0, listenerAt = 0;
    int editorTicks = 0, editorOnce = 0;
    int ticks = 0, oneShotCalls = 0, onceTimeouts = 0, disconnectable = 0, disconnectableAt = 0;
    std::size_t tickId = 0;
    std::set<std::string> passed;
};

REGISTER_SCRIPT(SignalTest)
