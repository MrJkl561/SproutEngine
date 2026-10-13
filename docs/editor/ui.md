# Menus and HUDs

Most games need some **UI**: a title menu, a score in the corner, a health
bar, a pause screen. Sprout has UI nodes for this, like Godot's **Control**
nodes: `Button`, `Panel`, `CheckBox`, `HSlider`, `ProgressBar`,
`TextureRect`, `LineEdit` and two containers that line things up. `Label`
(text) works with them too.

Every UI node is a rectangle: its **position** is the top-left corner and its
**size** is how big it is, in game pixels.

---

## Step 1: a HUD that stays on screen

When the camera follows the player, everything in the world moves across the
screen. A score shouldn't. That's what a **CanvasLayer** is for: whatever is
inside it stays put on the screen, whatever the camera does.

1. Select your scene's root node, press **Add** and pick **CanvasLayer**. Name
   it `Hud`.
2. With `Hud` selected, add a **Label**. Name it `Coins`, set its **text** to
   `Coins: 0` and its **position** to `10`, `10`.
3. Add a **ProgressBar** to `Hud` too. Name it `Health`, set its
   **position** to `10`, `40`, its **size** to `150`, `16`, and its **value**
   to `100`.

In the viewport, a CanvasLayer's nodes are drawn where they'll be on the
screen: the game window's frame starts at 0,0. Press **Play** and walk around:
the HUD stays in the corner.

To change them from a script:

```cpp
findNode<Label>("Coins")->text = "Coins: " + std::to_string(coins);
findNode<ProgressBar>("Health")->value = health;  // 0 to 100 (min_value / max_value)
```

> **Tip:** put the HUD in an **autoload** scene (gear button > Autoloads...)
> and it stays on screen across every level.

---

## Step 2: a menu with buttons

1. Make a new scene (**New**), add a **Node2D** as its root and call it
   `Title`. Add a **CanvasLayer** to it.
2. Add a **VBoxContainer** to the CanvasLayer, name it `Menu`, and set its
   **position** to `300`, `150` and its **size** to `200`, `0`. A
   VBoxContainer puts its children under each other and stretches them to its
   width, so you never line buttons up by hand.
3. Add three **Button**s to `Menu`: name them `Start`, `Options` and `Quit`,
   and set each one's **text**. Set their **size** y to `48` for big buttons.
   Change **separation** on `Menu` to space them out.
4. Give `Title` a script (**New** under Script in the Inspector, call it
   `TitleMenu`):

```cpp
#include "script.hpp"

using namespace s2d;

class TitleMenu : public Script {
public:
    void ready() override {
        connect(findNode("Start"), "pressed", [this] { changeScene("scenes/main.scene.json"); });
        connect(findNode("Quit"), "pressed", [this] { quit(); });
    }
};

REGISTER_SCRIPT(TitleMenu)
```

`pressed` is sent when a button is clicked: the mouse goes down *and* comes
back up on it. `quit()` ends the game (in the editor, it stops playing). You can also connect it without code: select the button, find
**Signals** in the Inspector and press **Connect...** next to `pressed`, like
in [Signals, step by step](signals.md).

Save the scene as `title.scene.json`, make it the main scene (in the Export
window), and press Play.

---

## Step 3: settings

An options screen uses the other UI nodes:

| Node | Signal | Example |
|---|---|---|
| `CheckBox` | `toggled(bool on)` | Music on or off |
| `HSlider` | `value_changed(float value)` | Volume, from `min_value` to `max_value` |
| `LineEdit` | `text_submitted(std::string text)` (Enter) | The player's name |

```cpp
void ready() override {
    connect(findNode("Volume"), "value_changed", [](float v) { SetMasterVolume(v / 100.0f); });
    connect(findNode("Music"), "toggled", [this](bool on) { musicOn = on; });
    connect(findNode("Name"), "text_submitted", [this](std::string name) { playerName = name; });
}
```

(Keep settings in an [autoload](../../editor/README.md#autoloads-global-scripts)
so every scene can read them.)

---

## Making it look like your game

- **Colors:** `Panel`, `Button` and `LineEdit` have **color**, **border
  color**, **border width** and **corner radius**. Buttons also have a
  **hover color** and a **pressed color**.
- **Pictures:** set a Panel's or Button's **texture** to a picture of a box
  (from a UI pack, say). It's stretched with **nine-slicing**: the corners
  keep their size, and only the edges and middle stretch, so one small
  picture makes boxes of any size. **patch_margin** is how many pixels the
  corners are.
- **TextureRect** shows a picture: an icon, a portrait, a logo. Its
  **stretch_mode** is `0` to stretch it to the rectangle, `1` to fit it
  without squashing, `2` to repeat it, and `3` to show it at its own size,
  centred.
- **Text size:** every node with text has a **font size**.

---

## Clicking in the game, not on the UI

When the player shoots by clicking, a click on a button shouldn't fire too.
`isMouseOverUi()` tells you when the mouse is over a UI node:

```cpp
if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !isMouseOverUi())
    shootAt(getGlobalMousePosition());
```

Use `getGlobalMousePosition()` for where the mouse is in the world (where
your nodes are) and `getMousePosition()` for where it is on the screen (where
the CanvasLayer's nodes are), instead of raylib's `GetMousePosition()`: the
game is scaled to fit its window, and to fit the viewport in the editor.

The full list of UI nodes and their signals is in the
[editor manual](../../editor/README.md#ui-buttons-menus-and-huds).
