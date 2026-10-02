#pragma once
// =============================================================================
//  sprout/input.hpp - keyboard, mouse and gamepad
//
//  Two ways to read input:
//
//  1) Ask about a key directly:
//        if (isKeyDown(Key::Space)) { ... }          // held down right now
//        if (isKeyPressed(Key::Space)) { ... }       // pressed THIS frame only
//
//  2) Use "actions" (like Godot's Input Map). An action is a name for
//     something the player does, and it can be bound to several keys/buttons:
//        bindAction("shoot", Key::X);
//        bindAction("shoot", MouseButton::Left);
//        bindAction("shoot", GamepadButton::A);
//        if (isActionPressed("shoot")) { ... }
//
//  These actions already exist and work with arrows, WASD and a gamepad:
//     "left", "right", "up", "down", "jump" (Space / gamepad A),
//     "pause" (P / gamepad Start)
//
//  Moving a player in 4 directions is just:
//        position += getVector("left", "right", "up", "down") * speed * dt;
// =============================================================================
#include <string>

#include "sprout/math.hpp"

namespace sprout {

/// Every key on the keyboard.
enum class Key : int {
    None = 0,
    Apostrophe = 39, Comma = 44, Minus = 45, Period = 46, Slash = 47,
    Zero = 48, One, Two, Three, Four, Five, Six, Seven, Eight, Nine,
    Semicolon = 59, Equal = 61,
    A = 65, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    LeftBracket = 91, Backslash = 92, RightBracket = 93, Grave = 96,
    Space = 32, Escape = 256, Enter = 257, Tab = 258, Backspace = 259,
    Insert = 260, Delete = 261,
    Right = 262, Left = 263, Down = 264, Up = 265,
    PageUp = 266, PageDown = 267, Home = 268, End = 269,
    CapsLock = 280, ScrollLock = 281, NumLock = 282, PrintScreen = 283, Pause = 284,
    F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    LeftShift = 340, LeftControl = 341, LeftAlt = 342, LeftSuper = 343,
    RightShift = 344, RightControl = 345, RightAlt = 346, RightSuper = 347,
    Menu = 348,
    Keypad0 = 320, Keypad1, Keypad2, Keypad3, Keypad4, Keypad5, Keypad6, Keypad7, Keypad8, Keypad9,
    KeypadDecimal = 330, KeypadDivide, KeypadMultiply, KeypadSubtract, KeypadAdd, KeypadEnter, KeypadEqual,
};

enum class MouseButton : int { Left = 0, Right = 1, Middle = 2 };

/// Gamepad buttons, named like an Xbox controller.
/// (On a PlayStation pad: A = Cross, B = Circle, X = Square, Y = Triangle.)
enum class GamepadButton : int {
    Unknown = 0,
    DPadUp, DPadRight, DPadDown, DPadLeft,
    Y, B, A, X,
    LeftBumper, LeftTrigger, RightBumper, RightTrigger,
    Select, Home, Start,
    LeftStick, RightStick,
};

/// Gamepad sticks and triggers. Sticks go from -1 to 1, triggers from 0 to 1.
enum class GamepadAxis : int { LeftX = 0, LeftY, RightX, RightY, LeftTrigger, RightTrigger };

// --- Keyboard ----------------------------------------------------------------
/// True while the key is held down.
bool isKeyDown(Key key);
/// True only on the frame the key was first pressed.
bool isKeyPressed(Key key);
/// True only on the frame the key was let go.
bool isKeyReleased(Key key);
/// Letters the player typed this frame (for name entry boxes etc.).
std::string typedText();

// --- Mouse -------------------------------------------------------------------
bool isMouseDown(MouseButton button = MouseButton::Left);
bool isMousePressed(MouseButton button = MouseButton::Left);
bool isMouseReleased(MouseButton button = MouseButton::Left);
/// Where the mouse is on the SCREEN (in pixels from the top-left of the window).
Vec2 mousePosition();
/// Where the mouse is in the WORLD (takes the current scene's camera into account).
Vec2 mouseWorldPosition();
/// How much the mouse wheel moved this frame (positive = up).
float mouseWheel();

// --- Gamepad -----------------------------------------------------------------
/// Gamepad number 0 is the first controller, 1 the second, and so on.
bool isGamepadConnected(int gamepad = 0);
bool isGamepadDown(GamepadButton button, int gamepad = 0);
bool isGamepadPressed(GamepadButton button, int gamepad = 0);
bool isGamepadReleased(GamepadButton button, int gamepad = 0);
/// Stick/trigger position. Tiny wobbles near the middle are ignored.
float gamepadAxis(GamepadAxis axis, int gamepad = 0);

// --- Actions -----------------------------------------------------------------
/// Adds a key to an action. An action can have as many keys/buttons as you like.
void bindAction(const std::string& action, Key key);
void bindAction(const std::string& action, MouseButton button);
void bindAction(const std::string& action, GamepadButton button, int gamepad = 0);
/// Binds a stick direction: bindAction("left", GamepadAxis::LeftX, -1) means "stick pushed left".
void bindAction(const std::string& action, GamepadAxis axis, int direction, int gamepad = 0);
/// Removes every key/button from an action.
void clearAction(const std::string& action);

/// True while any key/button of the action is held.
bool isActionDown(const std::string& action);
/// True only on the frame the action started.
bool isActionPressed(const std::string& action);
/// True only on the frame the action stopped.
bool isActionReleased(const std::string& action);

/// -1 if only the negative action is held, +1 if only the positive one is, otherwise 0.
///   float move = getAxis("left", "right");
float getAxis(const std::string& negative, const std::string& positive);
/// A direction from four actions, with length 0 or 1 (so diagonals aren't faster).
///   Vec2 dir = getVector("left", "right", "up", "down");
Vec2 getVector(const std::string& left, const std::string& right,
               const std::string& up, const std::string& down);

} // namespace sprout
