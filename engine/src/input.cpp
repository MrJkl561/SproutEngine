#include "sprout/input.hpp"

#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "internal.hpp"
#include "sprout/scene.hpp"

namespace sprout {

namespace {

constexpr float kStickDeadzone = 0.25f; // ignore tiny stick wobbles
constexpr float kStickPressed = 0.5f;   // stick pushed this far counts as "pressed"

struct Binding {
    enum class Kind { Key, Mouse, GamepadButton, GamepadAxis } kind;
    int code = 0;
    int gamepad = 0;
    int direction = 0; // for axes: -1 or +1
};

struct ActionState {
    std::vector<Binding> bindings;
    bool down = false;
    bool wasDown = false;
};

bool bindingDown(const Binding& binding) {
    if (!detail::windowIsOpen()) return false;
    switch (binding.kind) {
    case Binding::Kind::Key: return IsKeyDown(binding.code);
    case Binding::Kind::Mouse: return IsMouseButtonDown(binding.code);
    case Binding::Kind::GamepadButton:
        return IsGamepadAvailable(binding.gamepad) && IsGamepadButtonDown(binding.gamepad, binding.code);
    case Binding::Kind::GamepadAxis:
        if (!IsGamepadAvailable(binding.gamepad)) return false;
        return GetGamepadAxisMovement(binding.gamepad, binding.code) * static_cast<float>(binding.direction) >
               kStickPressed;
    }
    return false;
}

std::map<std::string, ActionState> defaultActions() {
    std::map<std::string, ActionState> actions;
    auto add = [&](const std::string& name, std::vector<Binding> bindings) { actions[name].bindings = std::move(bindings); };
    using K = Binding::Kind;
    add("left", {{K::Key, KEY_LEFT}, {K::Key, KEY_A}, {K::GamepadButton, GAMEPAD_BUTTON_LEFT_FACE_LEFT},
                 {K::GamepadAxis, GAMEPAD_AXIS_LEFT_X, 0, -1}});
    add("right", {{K::Key, KEY_RIGHT}, {K::Key, KEY_D}, {K::GamepadButton, GAMEPAD_BUTTON_LEFT_FACE_RIGHT},
                  {K::GamepadAxis, GAMEPAD_AXIS_LEFT_X, 0, +1}});
    add("up", {{K::Key, KEY_UP}, {K::Key, KEY_W}, {K::GamepadButton, GAMEPAD_BUTTON_LEFT_FACE_UP},
               {K::GamepadAxis, GAMEPAD_AXIS_LEFT_Y, 0, -1}});
    add("down", {{K::Key, KEY_DOWN}, {K::Key, KEY_S}, {K::GamepadButton, GAMEPAD_BUTTON_LEFT_FACE_DOWN},
                 {K::GamepadAxis, GAMEPAD_AXIS_LEFT_Y, 0, +1}});
    add("jump", {{K::Key, KEY_SPACE}, {K::GamepadButton, GAMEPAD_BUTTON_RIGHT_FACE_DOWN}});
    add("pause", {{K::Key, KEY_P}, {K::GamepadButton, GAMEPAD_BUTTON_MIDDLE_RIGHT}});
    return actions;
}

std::map<std::string, ActionState>& actions() {
    static std::map<std::string, ActionState> all = defaultActions();
    return all;
}

bool actionDownNow(const ActionState& state) {
    for (const Binding& binding : state.bindings)
        if (bindingDown(binding)) return true;
    return false;
}

ActionState* findAction(const std::string& name) {
    auto it = actions().find(name);
    if (it == actions().end()) {
        static std::map<std::string, bool> warned;
        if (!warned[name]) {
            warned[name] = true;
            detail::warn("there is no action called \"" + name + "\". Create it with bindAction(\"" + name +
                         "\", Key::...) first.");
        }
        return nullptr;
    }
    return &it->second;
}

} // namespace

namespace detail {
void beginInputFrame() {
    for (auto& [name, state] : actions()) {
        state.wasDown = state.down;
        state.down = actionDownNow(state);
    }
}
} // namespace detail

// --- Keyboard ------------------------------------------------------------------
bool isKeyDown(Key key) { return detail::windowIsOpen() && key != Key::None && IsKeyDown(static_cast<int>(key)); }
bool isKeyPressed(Key key) {
    return detail::windowIsOpen() && key != Key::None && IsKeyPressed(static_cast<int>(key));
}
bool isKeyReleased(Key key) {
    return detail::windowIsOpen() && key != Key::None && IsKeyReleased(static_cast<int>(key));
}

std::string typedText() {
    std::string result;
    if (!detail::windowIsOpen()) return result;
    for (int codepoint = GetCharPressed(); codepoint > 0; codepoint = GetCharPressed()) {
        int size = 0;
        const char* utf8 = CodepointToUTF8(codepoint, &size);
        result.append(utf8, static_cast<std::size_t>(size));
    }
    return result;
}

// --- Mouse ---------------------------------------------------------------------
bool isMouseDown(MouseButton button) { return detail::windowIsOpen() && IsMouseButtonDown(static_cast<int>(button)); }
bool isMousePressed(MouseButton button) {
    return detail::windowIsOpen() && IsMouseButtonPressed(static_cast<int>(button));
}
bool isMouseReleased(MouseButton button) {
    return detail::windowIsOpen() && IsMouseButtonReleased(static_cast<int>(button));
}
Vec2 mousePosition() { return detail::windowIsOpen() ? detail::fromRay(GetMousePosition()) : Vec2{}; }

Vec2 mouseWorldPosition() {
    if (Scene* scene = detail::activeScene()) return scene->camera.screenToWorld(mousePosition());
    return mousePosition();
}

float mouseWheel() { return detail::windowIsOpen() ? GetMouseWheelMove() : 0.0f; }

// --- Gamepad -------------------------------------------------------------------
bool isGamepadConnected(int gamepad) { return detail::windowIsOpen() && IsGamepadAvailable(gamepad); }
bool isGamepadDown(GamepadButton button, int gamepad) {
    return isGamepadConnected(gamepad) && IsGamepadButtonDown(gamepad, static_cast<int>(button));
}
bool isGamepadPressed(GamepadButton button, int gamepad) {
    return isGamepadConnected(gamepad) && IsGamepadButtonPressed(gamepad, static_cast<int>(button));
}
bool isGamepadReleased(GamepadButton button, int gamepad) {
    return isGamepadConnected(gamepad) && IsGamepadButtonReleased(gamepad, static_cast<int>(button));
}
float gamepadAxis(GamepadAxis axis, int gamepad) {
    if (!isGamepadConnected(gamepad)) return 0.0f;
    const float value = GetGamepadAxisMovement(gamepad, static_cast<int>(axis));
    const bool isTrigger = axis == GamepadAxis::LeftTrigger || axis == GamepadAxis::RightTrigger;
    if (!isTrigger && std::abs(value) < kStickDeadzone) return 0.0f;
    return value;
}

// --- Actions -------------------------------------------------------------------
void bindAction(const std::string& action, Key key) {
    actions()[action].bindings.push_back({Binding::Kind::Key, static_cast<int>(key)});
}
void bindAction(const std::string& action, MouseButton button) {
    actions()[action].bindings.push_back({Binding::Kind::Mouse, static_cast<int>(button)});
}
void bindAction(const std::string& action, GamepadButton button, int gamepad) {
    actions()[action].bindings.push_back({Binding::Kind::GamepadButton, static_cast<int>(button), gamepad});
}
void bindAction(const std::string& action, GamepadAxis axis, int direction, int gamepad) {
    actions()[action].bindings.push_back(
        {Binding::Kind::GamepadAxis, static_cast<int>(axis), gamepad, direction < 0 ? -1 : 1});
}
void clearAction(const std::string& action) {
    ActionState& state = actions()[action];
    state.bindings.clear();
    state.down = state.wasDown = false;
}

bool isActionDown(const std::string& action) {
    ActionState* state = findAction(action);
    return state && actionDownNow(*state);
}
bool isActionPressed(const std::string& action) {
    ActionState* state = findAction(action);
    return state && state->down && !state->wasDown;
}
bool isActionReleased(const std::string& action) {
    ActionState* state = findAction(action);
    return state && !state->down && state->wasDown;
}

float getAxis(const std::string& negative, const std::string& positive) {
    float value = 0.0f;
    if (isActionDown(negative)) value -= 1.0f;
    if (isActionDown(positive)) value += 1.0f;
    return value;
}

Vec2 getVector(const std::string& left, const std::string& right, const std::string& up, const std::string& down) {
    return Vec2{getAxis(left, right), getAxis(up, down)}.normalized();
}

} // namespace sprout
