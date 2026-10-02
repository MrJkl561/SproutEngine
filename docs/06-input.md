# 6. Input: keyboard, mouse and gamepad

**Input** is everything the player does to control the game: pressing keys,
moving and clicking the mouse, pushing a gamepad stick. You read input in
`update()`.

Sprout gives you two ways to read input:

1. **Ask about a specific key or button**: "Is Space held down?"
2. **Use actions**: "Is the player pressing *jump*?", where *jump* can be Space,
   W or the gamepad's A button. This is the easier way to support several
   keys and gamepads at once.

---

## Keys

```cpp
if (isKeyDown(Key::Right)) position.x += 200 * dt;  // held down right now
if (isKeyPressed(Key::Space)) jump();               // pressed THIS frame
if (isKeyReleased(Key::LeftShift)) speed = 200;     // let go THIS frame
```

The difference between `Down` and `Pressed` is important:

| Function | True when... | Good for |
|---|---|---|
| `isKeyDown(key)` | the key is being held, on **every** frame | walking, running, holding a shield |
| `isKeyPressed(key)` | the key **went down** this frame (once per press) | jumping, firing a single shot, menus, "press Enter" |
| `isKeyReleased(key)` | the key **came up** this frame (once per release) | charging a jump while held and releasing it |

> **Example:** if you used `isKeyDown(Key::Enter)` for "press Enter to start", it
> would also be true on the next frame of the same press, and might skip straight
> through two menus! Use `isKeyPressed` for things that should happen once.

### Key names

All the keys are in `Key::`. The common ones:

| Keys | Names |
|---|---|
| Letters | `Key::A` ... `Key::Z` |
| Number row | `Key::Zero`, `Key::One` ... `Key::Nine` |
| Arrows | `Key::Up`, `Key::Down`, `Key::Left`, `Key::Right` |
| Common keys | `Key::Space`, `Key::Enter`, `Key::Escape`, `Key::Tab`, `Key::Backspace`, `Key::Delete` |
| Modifiers | `Key::LeftShift`, `Key::RightShift`, `Key::LeftControl`, `Key::RightControl`, `Key::LeftAlt`, `Key::RightAlt` |
| Function keys | `Key::F1` ... `Key::F12` |
| Number pad | `Key::Keypad0` ... `Key::Keypad9`, `Key::KeypadEnter`, `Key::KeypadAdd`, `Key::KeypadSubtract` |
| Symbols | `Key::Minus`, `Key::Equal`, `Key::Comma`, `Key::Period`, `Key::Slash`, `Key::Semicolon`, `Key::Apostrophe`, `Key::LeftBracket`, `Key::RightBracket`, `Key::Backslash`, `Key::Grave` |
| Others | `Key::Insert`, `Key::Home`, `Key::End`, `Key::PageUp`, `Key::PageDown`, `Key::CapsLock`, `Key::PrintScreen`, `Key::Pause`, ... |

> **Esc quits the game** by default. To use Esc for your own pause menu, turn
> that off with `setExitKey(Key::None);` (or pick another key:
> `setExitKey(Key::F10);`). Call it after the `Game` is created, for example in
> `main()` or in your first scene's `start()`.

> **Free screenshots:** press **F12** in any Sprout game to save a screenshot
> (`screenshot000.png`, `screenshot001.png`, ...) in the game's folder, next to the
> program. This comes from raylib.

### Typing text

`typedText()` gives you the letters the player typed this frame (usually zero or
one character). Add them to a string to let the player type a name:

```cpp
std::string name; // a member of your scene

void update(float dt) override {
    name += typedText();
    if (isKeyPressed(Key::Backspace) && !name.empty()) name.pop_back();
}
```

`!name.empty()` means "the name isn't empty", and `pop_back()` removes the last
letter. If you want a ready-made text box with a frame and a cursor, use
`textBox` from [Menus and UI](12-menus-and-ui.md) (don't use both at the same
time, because they'd steal each other's letters).

---

## Actions

An **action** is a name for something the player does, such as `"jump"`,
`"shoot"` or `"left"`. Each action can have as many keys and buttons as you
like. This idea comes from the Godot engine's "Input Map".

Why bother? Because then your game code just asks `isActionPressed("jump")`,
and it automatically works with Space **and** W **and** a gamepad, without a
long list of `||` ("or") checks everywhere.

### Actions that already exist

| Action | Keyboard | Gamepad |
|---|---|---|
| `"left"` | Left arrow, A | D-pad left, left stick left |
| `"right"` | Right arrow, D | D-pad right, left stick right |
| `"up"` | Up arrow, W | D-pad up, left stick up |
| `"down"` | Down arrow, S | D-pad down, left stick down |
| `"jump"` | Space | A (Cross on PlayStation) |
| `"pause"` | P | Start |

(The gamepad ones use the first gamepad.) Nothing happens automatically when
these are pressed. For example, `"pause"` doesn't pause your game by itself, you
decide what it does (see [Pausing](04-scenes.md#pausing)).

### Reading actions

```cpp
if (isActionDown("left")) ...      // held right now
if (isActionPressed("jump")) ...   // started this frame
if (isActionReleased("jump")) ...  // stopped this frame
```

These work exactly like `isKeyDown`, `isKeyPressed` and `isKeyReleased`.

### Moving with actions: `getAxis` and `getVector`

These two make movement code really short:

```cpp
// Left and right only (a platformer, a paddle, a basket):
float move = getAxis("left", "right");   // -1, 0 or +1
position.x += move * speed * dt;

// All four directions (top-down games):
Vec2 direction = getVector("left", "right", "up", "down");
position += direction * speed * dt;
```

- `getAxis(negative, positive)` gives **-1** while only the first action is
  held, **+1** while only the second is held, and **0** otherwise.
- `getVector(left, right, up, down)` gives a direction `Vec2`. It has a
  **length of 1** (or 0 when nothing is held), so going diagonally isn't faster
  than going straight. Without that, diagonal movement would be about 41% faster.
  Players notice that kind of thing!

### Making your own actions

`bindAction(name, key or button)` adds a key or button to an action. If the
action doesn't exist yet, it's created:

```cpp
int main() {
    Game game("Space Shooter", 800, 450);

    bindAction("shoot", Key::X);
    bindAction("shoot", Key::LeftControl);
    bindAction("shoot", MouseButton::Left);
    bindAction("shoot", GamepadButton::RightTrigger);

    game.run<PlayScene>();
}
```

Now `isActionDown("shoot")` is true while any of those four is held.

- Actions are shared by the whole game and kept when scenes change. The best
  place to set them up is `main()`, once, before `game.run`.
- You can also add keys to the ready-made actions:
  `bindAction("jump", Key::W);` makes W jump too.
- `clearAction("jump")` removes **every** key and button from an action, so you
  can set it up from scratch (handy for a "change controls" menu).
- If you misspell an action name (`isActionDown("shoto")`), Sprout prints a
  warning in the terminal, once, telling you that action doesn't exist.

`bindAction` accepts:

| What | Example |
|---|---|
| A key | `bindAction("shoot", Key::X);` |
| A mouse button | `bindAction("shoot", MouseButton::Left);` |
| A gamepad button | `bindAction("shoot", GamepadButton::X);` |
| A gamepad button on gamepad number 1 (the second one) | `bindAction("p2_shoot", GamepadButton::X, 1);` |
| A stick direction (-1 or +1) | `bindAction("p2_left", GamepadAxis::LeftX, -1, 1);` |

### Two players on one keyboard

Make separate actions for each player:

```cpp
// in main(), before game.run:
bindAction("p1_up", Key::W);
bindAction("p1_down", Key::S);
bindAction("p2_up", Key::Up);
bindAction("p2_down", Key::Down);

// in the left paddle's update():
position.y += getAxis("p1_up", "p1_down") * speed * dt;
// in the right paddle's update():
position.y += getAxis("p2_up", "p2_down") * speed * dt;
```

(The ready-made `"up"` action has **both** W and the up arrow, which is why we
don't use it for a two-player game.)

---

## Mouse

```cpp
if (isMousePressed()) ...                      // left button clicked this frame
if (isMouseDown(MouseButton::Right)) ...       // right button held
if (isMouseReleased(MouseButton::Middle)) ...  // middle button (wheel) let go

Vec2 onScreen = mousePosition();        // where the mouse is on the window
Vec2 inWorld = mouseWorldPosition();    // where the mouse is in the game world
float wheel = mouseWheel();             // wheel movement this frame (up is positive)
```

The mouse functions use the left button if you don't say which one.

**Screen or world?** If your camera never moves, both positions are the same.
If the camera follows the player or zooms, use:

- `mousePosition()` for things in `drawUI()`, like buttons and a custom cursor
  (screen coordinates),
- `mouseWorldPosition()` for things in the game world, like "click on an enemy"
  or "shoot towards the mouse" (world coordinates).

```cpp
// Shoot towards the mouse:
Vec2 aim = position.directionTo(mouseWorldPosition());
scene().spawn<Bullet>(position, aim * 600);
```

Want your own cursor picture? Call `hideCursor()` and draw something at
`mousePosition()` in `drawUI()`. `showCursor()` brings the normal one back.

---

## Gamepads

Plug in a controller and the ready-made actions already work with it. To read it
directly:

```cpp
if (isGamepadConnected()) ...                        // is gamepad 0 plugged in?
if (isGamepadPressed(GamepadButton::A)) ...           // A pressed this frame
if (isGamepadDown(GamepadButton::RightBumper)) ...    // held
if (isGamepadReleased(GamepadButton::Start)) ...      // let go this frame

float x = gamepadAxis(GamepadAxis::LeftX);   // -1 (left) ... 0 ... +1 (right)
float y = gamepadAxis(GamepadAxis::LeftY);   // -1 (up)   ... 0 ... +1 (down)
```

Every gamepad function takes an optional gamepad number at the end: `0` is the
first controller (the default), `1` the second, and so on:
`isGamepadDown(GamepadButton::A, 1)`.

The buttons are named like on an Xbox controller:

| Sprout name | Xbox | PlayStation | Nintendo Switch position |
|---|---|---|---|
| `GamepadButton::A` | A | Cross ✕ | bottom |
| `GamepadButton::B` | B | Circle ○ | right |
| `GamepadButton::X` | X | Square □ | left |
| `GamepadButton::Y` | Y | Triangle △ | top |
| `LeftBumper`, `RightBumper` | LB, RB | L1, R1 | L, R |
| `LeftTrigger`, `RightTrigger` | LT, RT | L2, R2 | ZL, ZR |
| `DPadUp`, `DPadDown`, `DPadLeft`, `DPadRight` | D-pad | D-pad | D-pad |
| `Start`, `Select`, `Home` | Menu, View, Xbox | Options, Share, PS | +, -, Home |
| `LeftStick`, `RightStick` | pressing the sticks in | L3, R3 | pressing the sticks in |

The sticks are `GamepadAxis::LeftX`, `LeftY`, `RightX` and `RightY`. Tiny
wobbles near the middle are ignored for you, so a stick at rest gives exactly 0.
Remember y goes **down**, so pushing a stick down gives a positive y.

The analog triggers are `GamepadAxis::LeftTrigger` and `RightTrigger`, and they
go up to 1 when pressed all the way. The simplest way to use a trigger is as a
button (`GamepadButton::RightTrigger`), or check
`gamepadAxis(GamepadAxis::RightTrigger) > 0.5f` for "pressed more than halfway".

Moving with the left stick, with the analog "push a little to walk slowly" feel:

```cpp
Vec2 stick{gamepadAxis(GamepadAxis::LeftX), gamepadAxis(GamepadAxis::LeftY)};
position += stick * speed * dt;
```

---

## A complete example: input tester

Run this to try every kind of input, including your gamepad.

```cpp
#include <sprout/sprout.hpp>

using namespace sprout;

struct InputTester : Scene {
    Vec2 player{400, 250};
    std::string typed;

    void update(float dt) override {
        // Arrow keys, WASD, the D-pad or the left stick
        player += getVector("left", "right", "up", "down") * 250 * dt;

        // Our own action (see main() below)
        if (isActionPressed("shoot")) particles.burst(player, Color::Orange);

        // Right click teleports the player to the mouse
        if (isMousePressed(MouseButton::Right)) player = mouseWorldPosition();

        // Typing
        typed += typedText();
        if (isKeyPressed(Key::Backspace) && !typed.empty()) typed.pop_back();
    }

    void draw() override {
        drawCircle(player, 20, isActionDown("jump") ? Color::Yellow : Color::SkyBlue);
    }

    void drawUI() override {
        drawText("Move: arrows / WASD / gamepad.   Jump (Space) turns you yellow.", 10, 10, 20);
        drawText("Shoot: X, left click or gamepad X.   Right click: teleport.", 10, 35, 20);
        drawText("You typed: " + typed, 10, 60, 20, Color::Gold);
        drawText(isGamepadConnected() ? "Gamepad connected!" : "No gamepad found", 10, 85, 20, Color::LightGray);

        // A ring around the mouse that turns red while the button is held
        drawCircleOutline(mousePosition(), 10, isMouseDown() ? Color::Red : Color::White);
    }
};

int main() {
    Game game("Input tester", 800, 450);

    // Make a new action called "shoot" and give it three buttons
    bindAction("shoot", Key::X);
    bindAction("shoot", MouseButton::Left);
    bindAction("shoot", GamepadButton::X);

    game.run<InputTester>();
}
```

`condition ? a : b` means "a if the condition is true, otherwise b". It's a
compact `if` that gives back a value.

---

## Quick reference

| Code | What it does |
|---|---|
| `isKeyDown(Key::A)` / `isKeyPressed(...)` / `isKeyReleased(...)` | Keyboard |
| `typedText()` | Letters typed this frame |
| `isMouseDown()` / `isMousePressed()` / `isMouseReleased()` | Mouse buttons (left unless you pass `MouseButton::Right` or `Middle`) |
| `mousePosition()` / `mouseWorldPosition()` / `mouseWheel()` | Mouse position and wheel |
| `isGamepadConnected(n)` | Is gamepad `n` plugged in? |
| `isGamepadDown(GamepadButton::A, n)` / `Pressed` / `Released` | Gamepad buttons |
| `gamepadAxis(GamepadAxis::LeftX, n)` | Sticks and triggers |
| `bindAction("name", key/button/axis)` | Adds a key or button to an action |
| `clearAction("name")` | Removes all keys and buttons from an action |
| `isActionDown("name")` / `isActionPressed(...)` / `isActionReleased(...)` | Reading actions |
| `getAxis("left", "right")` | -1, 0 or +1 |
| `getVector("left", "right", "up", "down")` | A direction of length 0 or 1 |
| `setExitKey(Key::None)` | Stop Esc from quitting |
| `hideCursor()` / `showCursor()` | The mouse pointer |

**Next:** [Drawing](07-drawing.md): shapes, text, pictures and animation.
