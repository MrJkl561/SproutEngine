# 12. Menus and UI

**UI** stands for **user interface**: the buttons, sliders, checkboxes and text
boxes of your menus. Sprout's UI comes from the
[raygui](https://github.com/raysan5/raygui) library.

The UI is **immediate mode**, which is a fancy way of saying: you call one
function every frame, and it both **draws** the control and **tells you** what
the player did with it. There are no button objects to create or clean up.

```cpp
void drawUI() override {
    if (button("Play", {300, 200, 200, 50})) changeScene<Level1>();
    if (button("Quit", {300, 270, 200, 50})) quitGame();
}
```

That's a whole working menu! Each frame, `button` draws the button, and gives
`true` on the frame the player clicks it.

Two rules:

1. Call UI functions in **`drawUI()`**. That way they're drawn on top and the
   camera doesn't move them.
2. Every control needs a **`Rect`** that says where it goes on the screen:
   `{x, y, width, height}`, with x and y being the **top-left corner**.

---

## The controls

| Function | What it does | Gives back |
|---|---|---|
| `button(text, area)` | A clickable button | `true` on the frame it's clicked |
| `checkbox(text, area, checked)` | A box you tick and untick. Changes the `bool` you give it. | `true` if it changed |
| `slider(text, area, value, min, max)` | Drag to pick a number from `min` to `max`. Changes the `float` you give it. | `true` if it changed |
| `textBox(area, text, maxLength = 32)` | A box the player can type in. Changes the `std::string` you give it. | `true` if the text changed |
| `progressBar(area, value, max = 1)` | A bar that's `value / max` full (health, loading...) | nothing |
| `label(text, area)` | Plain text in the UI's style | nothing |
| `panel(title, area)` | A box with a title bar, to group other controls | nothing |

### Buttons

```cpp
if (button("Start", {300, 200, 200, 50})) {
    changeScene<Level1>();
}
```

The text is centered on the button. A button lights up when the mouse is over
it and gives `true` when it's clicked.

### Checkboxes

```cpp
bool showFPS = false; // a member of your scene (or a global variable)

void drawUI() override {
    checkbox("Show FPS", {20, 20, 24, 24}, showFPS);
    if (showFPS) drawFPS(700, 10);
}
```

The `area` is the little box itself. The text is drawn to its **right**. Each
click flips the `bool` between `true` and `false`. The `bool` must be a variable
that lives on between frames (a member of your scene, not a variable made inside
`drawUI()`), otherwise it would be reset every frame.

To do something at the moment it changes, use the value it gives back:

```cpp
if (checkbox("Fullscreen", {230, 135, 24, 24}, fullscreen)) {
    setFullscreen(fullscreen);
}
```

### Sliders

```cpp
float volume = 1.0f; // a member

void drawUI() override {
    if (slider("Volume", {310, 90, 200, 24}, volume, 0.0f, 1.0f)) {
        setMasterVolume(volume);
    }
}
```

The slider changes `volume` while the player drags it. Its text is drawn to the
**left** of the `area`, and the current value (like `0.75`) to the **right**, so
leave some room on both sides.

> **Neater forms:** the slider's own text sits right up against the bar. For a
> tidy settings screen where everything lines up, give the slider an empty text
> `""` and put a `label` beside it, like `examples/09_menus_and_settings` does:
>
> ```cpp
> label("Volume", {180, 145, 130, 30});
> slider("", {320, 150, 220, 20}, volume, 0.0f, 1.0f);
> ```

### Text boxes

```cpp
std::string name = "Player"; // a member

void drawUI() override {
    label("Your name:", {230, 180, 120, 30});
    textBox({350, 180, 220, 30}, name, 16); // at most 16 letters
}
```

Click the box to start typing. Press **Enter** (or click somewhere else) to
finish. While a text box is active, don't also read `typedText()`, because they
would steal each other's letters.

> **Good to know:** Esc normally closes the game, but not while the player is
> typing in a text box, so nobody loses their game by accident. (To change or
> turn off the quit key, see `setExitKey` in [Input](06-input.md#key-names).)

### Progress bars

```cpp
progressBar({20, 20, 200, 20}, health, maxHealth);   // e.g. 30 of 100 = 30% full
progressBar({250, 220, 300, 30}, loading.progress()); // 0.0 to 1.0
```

A `Timer`'s `progress()` goes from 0 to 1, which is exactly what a progress bar
wants (see [Timers](11-timers.md)).

### Labels and panels

```cpp
panel("Settings", {200, 40, 400, 370});        // a box with a title bar
label("Your name:", {230, 180, 120, 30});       // text, centered up and down in its area
```

Draw the panel **first**, then the controls on top of it.

> `label` uses the UI's look. For big titles and scores, `drawText` and
> `drawTextCentered` are usually nicer (see [Drawing](07-drawing.md#text)).

---

## Looks: text size and themes

```cpp
setUITextSize(24);           // bigger UI text (20 is the default)
setUITheme(UITheme::Dark);   // a different look for every control
```

The themes are: `UITheme::Default`, `Dark`, `Candy`, `Cyber`, `Jungle`, `Sunny`
and `Terminal`. Try them all!

Each theme brings its own pixel font and its own text size (most are a bit
smaller than 20). Once you've picked a size with `setUITextSize`, Sprout keeps
that size, even when you change the theme later:

```cpp
setUITextSize(20);           // keep the text this big...
setUITheme(UITheme::Candy);  // ...whatever the theme
```

Themes and text size apply to the whole game until you change them again. A
good place to set them is `main()` (after the `Game` is created) or your first
scene's `start()`.

> **Candy theme quirk:** its font draws `>` like `<` (a little bug in raygui), so
> avoid those two characters on Candy buttons. Write "Next" instead of ">".

---

## Laying out a menu

Some tips for neat menus:

- Center things: a button 200 wide in an 800-wide window starts at
  `x = (800 - 200) / 2 = 300`.
- Space buttons evenly: if they're 50 tall with a 15-pixel gap, each one goes
  65 pixels lower than the last (`y = 180`, `245`, `310`...).
- Make buttons big enough for their text. At the default size 20, each letter
  is roughly 10 to 12 pixels wide.

Or let a loop do the math for you:

```cpp
std::vector<std::string> levels = {"Forest", "Cave", "Castle"};
for (int i = 0; i < 3; i++) {
    if (button(levels[i], {300, 150.0f + i * 65, 200, 50})) {
        changeScene<Level>(i + 1);
    }
}
```

---

## A complete example: main menu and settings

Three scenes: a main menu, a settings screen with every kind of control, and a
"game" that loads and then greets you by name.

```cpp
#include <sprout/sprout.hpp>

using namespace sprout;

// Settings live outside the scenes, so they survive changing scenes.
// (To keep them after the game closes, see the Saving page.)
float volume = 1.0f;
bool fullscreen = false;
std::string playerName = "Sam";

// --- The main menu ------------------------------------------------------------------
struct MainMenu : Scene {
    void start() override {
        background = Color{30, 34, 45};
    }

    void drawUI() override; // written at the bottom of the file
};

// --- The settings menu --------------------------------------------------------------
struct SettingsMenu : Scene {
    void start() override {
        background = Color{30, 34, 45};
    }

    void drawUI() override {
        panel("Settings", {200, 40, 400, 370});

        // Sliders show their label on the LEFT, so leave some room there
        if (slider("Volume", {310, 90, 200, 24}, volume, 0.0f, 1.0f)) setMasterVolume(volume);

        if (checkbox("Fullscreen", {230, 135, 24, 24}, fullscreen)) setFullscreen(fullscreen);

        label("Your name:", {230, 180, 120, 30});
        textBox({350, 180, 220, 30}, playerName, 16);

        label("Theme:", {230, 230, 100, 30});
        if (button("Default", {228, 265, 84, 30})) setUITheme(UITheme::Default);
        if (button("Dark", {314, 265, 84, 30})) setUITheme(UITheme::Dark);
        if (button("Candy", {400, 265, 84, 30})) setUITheme(UITheme::Candy);
        if (button("Cyber", {486, 265, 84, 30})) setUITheme(UITheme::Cyber);

        if (button("Back", {300, 340, 200, 45})) changeScene<MainMenu>();
    }
};

// --- The "game" -----------------------------------------------------------------------
struct PlayScene : Scene {
    Timer loading{3.0f};

    void drawUI() override {
        if (!loading.isDone()) {
            drawTextCentered("Loading...", {400, 180}, 30);
            progressBar({250, 220, 300, 30}, loading.progress());
            return;
        }
        drawTextCentered("Hello, " + playerName + "!", {400, 180}, 40, Color::Gold);
        if (button("Back to the menu", {275, 250, 250, 50})) changeScene<MainMenu>();
    }
};

// MainMenu's drawUI() is down here because it needs SettingsMenu and PlayScene.
void MainMenu::drawUI() {
    drawTextCentered("MY GAME", {400, 100}, 60, Color::Gold);
    if (button("Play", {300, 180, 200, 50})) changeScene<PlayScene>();
    if (button("Settings", {300, 245, 200, 50})) changeScene<SettingsMenu>();
    if (button("Quit", {300, 310, 200, 50})) quitGame();
}

int main() {
    Game game("Menus", 800, 450);
    setUITextSize(20); // keep this text size, whichever theme is picked
    game.run<MainMenu>();
}
```

Why is `MainMenu::drawUI()` at the bottom? The main menu switches to
`SettingsMenu` and `PlayScene`, and they switch back to `MainMenu`, so they all
need each other. The trick is explained in
[Scenes](04-scenes.md#when-two-scenes-need-each-other).

> **Try it!** Add a "Hard mode" checkbox and make it change something in the
> game. Then make the settings survive closing the game using a `SaveFile`
> ([Saving](13-saving.md)). Look at `examples/09_menus_and_settings` for a bigger
> example.

---

## A pause menu

Combining [pausing](04-scenes.md#pausing) with buttons:

```cpp
void update(float dt) override {
    if (isActionPressed("pause")) paused = !paused;
    if (paused) return;
    // ...the game...
}

void drawUI() override {
    // ...score etc...
    if (paused) {
        drawRect(0, 0, screenWidth(), screenHeight(), Color::Black.withAlpha(0.5f));
        if (button("Resume", {300, 150, 200, 50})) paused = false;
        if (button("Restart", {300, 215, 200, 50})) restartScene();
        if (button("Quit", {300, 280, 200, 50})) quitGame();
    }
}
```

**Next:** [Saving](13-saving.md).
