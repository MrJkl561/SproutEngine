// Tests for sprout/input.hpp WITHOUT a window: nothing can be pressed, but
// setting up actions must work, and asking about them must be safe.
#include <doctest/doctest.h>

#include <string>

#include <sprout/input.hpp>

#include "test_helpers.hpp"

using namespace sprout;
using sprout_test::CaptureStderr;

TEST_CASE("binding and clearing actions works before the window exists") {
    CHECK_NOTHROW(bindAction("shoot", Key::X));
    CHECK_NOTHROW(bindAction("shoot", MouseButton::Left));
    CHECK_NOTHROW(bindAction("shoot", GamepadButton::A));
    CHECK_NOTHROW(bindAction("shoot", GamepadButton::RightTrigger, 1));
    CHECK_NOTHROW(bindAction("aim", GamepadAxis::RightX, +1));
    CHECK_NOTHROW(bindAction("aim", GamepadAxis::RightX, -1, 2));

    CHECK_FALSE(isActionDown("shoot"));
    CHECK_FALSE(isActionPressed("shoot"));
    CHECK_FALSE(isActionReleased("shoot"));

    CHECK_NOTHROW(clearAction("shoot"));
    CHECK_FALSE(isActionDown("shoot"));
    CHECK_NOTHROW(clearAction("an action that never existed"));
}

TEST_CASE("the built-in actions exist and are not pressed without a window") {
    CaptureStderr captured;
    for (const char* action : {"left", "right", "up", "down", "jump", "pause"}) {
        CAPTURE(action);
        CHECK_FALSE(isActionDown(action));
        CHECK_FALSE(isActionPressed(action));
        CHECK_FALSE(isActionReleased(action));
    }
    CHECK(captured.text().empty()); // no "unknown action" warnings for these
}

TEST_CASE("an unknown action name is false and warns only once") {
    CaptureStderr captured;
    CHECK_FALSE(isActionDown("jupm")); // typo!
    CHECK_FALSE(isActionPressed("jupm"));
    CHECK_FALSE(isActionReleased("jupm"));
    CHECK_FALSE(isActionDown("jupm"));
    CHECK(captured.count("jupm") >= 1);
    CHECK(captured.count("Warning") == 1); // one warning, not one every frame
    CHECK(captured.text().find("bindAction") != std::string::npos); // and it says how to fix it
}

TEST_CASE("getAxis and getVector are zero without a window") {
    CHECK(getAxis("left", "right") == 0.0f);
    CHECK(getAxis("up", "down") == 0.0f);
    const Vec2 direction = getVector("left", "right", "up", "down");
    CHECK(direction == Vec2{0, 0});
}

TEST_CASE("keyboard, mouse and gamepad queries are safe without a window") {
    CHECK_FALSE(isKeyDown(Key::Space));
    CHECK_FALSE(isKeyPressed(Key::A));
    CHECK_FALSE(isKeyReleased(Key::Enter));
    CHECK_FALSE(isKeyDown(Key::None));
    CHECK(typedText().empty());

    CHECK_FALSE(isMouseDown());
    CHECK_FALSE(isMousePressed(MouseButton::Right));
    CHECK_FALSE(isMouseReleased(MouseButton::Middle));
    CHECK(mousePosition() == Vec2{0, 0});
    CHECK(mouseWorldPosition() == Vec2{0, 0});
    CHECK(mouseWheel() == 0.0f);

    CHECK_FALSE(isGamepadConnected());
    CHECK_FALSE(isGamepadDown(GamepadButton::A));
    CHECK_FALSE(isGamepadPressed(GamepadButton::Start, 1));
    CHECK_FALSE(isGamepadReleased(GamepadButton::B));
    CHECK(gamepadAxis(GamepadAxis::LeftX) == 0.0f);
    CHECK(gamepadAxis(GamepadAxis::RightTrigger, 3) == 0.0f);
}
