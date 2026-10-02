# 3. How a game works

This page explains what's going on "under the hood" while your game runs. Once
you understand the **game loop**, **frames**, **delta time** and
**coordinates**, the rest of Sprout will make a lot more sense.

---

## The game loop

A game is like a **flip book**. Each page is a slightly different drawing, and
when you flip through them quickly, things seem to move. In a game, each page is
called a **frame**, and Sprout draws about **60 frames every second**.

To make each frame, Sprout runs the **game loop**. In plain English, it looks
like this:

```
open the window
make the first scene and run its start()

repeat until the window is closed:
    1. how much time passed since the last frame?   -> dt
    2. read the keyboard, mouse and gamepads
    3. UPDATE: move everything a tiny bit, check collisions, keep score...
    4. DRAW:   wipe the screen, then draw everything in its new place
    5. show the finished picture, then wait a moment so we stay at 60 frames per second
```

You never write this loop yourself. Sprout runs it for you, and **calls your
functions** at the right moments:

| Your function | When Sprout calls it | What you do in it |
|---|---|---|
| `start()` | Once, when the scene (or entity) begins | Set things up: spawn the player, set the background |
| `update(float dt)` | Every frame, before drawing | Move things, read input, check collisions, change the score |
| `draw()` | Every frame, after updating | Draw the game world (seen through the [camera](10-camera-and-effects.md)) |
| `drawUI()` | Every frame, after `draw()` | Draw the score, health bars and menus on top. The camera doesn't move these. |

> **Golden rule:** change things in `update()`, draw things in `draw()` /
> `drawUI()`. Drawing in `update()` won't show up, because the screen is wiped
> before drawing starts. And changing things in `draw()` makes your game harder to
> understand.

---

## Delta time (`dt`)

Every `update` gets a number called `dt`, short for **delta time**. ("Delta" is
a math word for "change", so it's the change in time.) It's the number of
**seconds since the last frame**.

At 60 frames per second, `dt` is about `1 / 60 = 0.0167` seconds.

### Why you need it

Imagine you move a player like this:

```cpp
position.x += 5; // 5 pixels every frame
```

On a computer running at 60 frames per second, that's `5 × 60 = 300` pixels per
second. But if the game slows down to 30 frames per second (an older laptop,
lots of enemies on screen...), it's only `5 × 30 = 150` pixels per second. Your
player is suddenly **half as fast**!

Now do it with `dt`:

```cpp
position.x += 300 * dt; // 300 pixels every SECOND
```

| Frames per second | `dt` | Pixels per frame (`300 * dt`) | Pixels per second |
|---|---|---|---|
| 60 | 0.0167 | 5 | 5 × 60 = **300** |
| 30 | 0.0333 | 10 | 10 × 30 = **300** |
| 20 | 0.05 | 15 | 15 × 20 = **300** |

When the game runs slowly, `dt` is bigger, so each step is bigger, and the
player moves at the same speed in the end. **Multiply anything that changes over
time by `dt`**: movement, falling, spinning, timers you count yourself,
speeding up...

```cpp
position += velocity * dt;   // moving
velocity.y += gravity * dt;  // falling faster and faster
angle += 90 * dt;            // spinning 90 degrees per second
starSpeed += 5 * dt;         // getting harder by 5 every second
```

Things that happen **once** (like "add 1 to the score when you catch a coin") do
**not** use `dt`.

> **Safety net:** if a frame takes a long time (for example while you drag the
> window around), Sprout never makes `dt` bigger than `0.1` seconds. Otherwise
> fast things could jump straight through walls.

### See it for yourself

This little program moves two squares. The red one forgets `dt`, while the green
one uses it. Press **F** to switch the game between 60 and 20 frames per second
and watch what happens.

```cpp
#include <sprout/sprout.hpp>

using namespace sprout;

struct DeltaTimeDemo : Scene {
    float redX = 0;   // moves 5 pixels every FRAME
    float greenX = 0; // moves 300 pixels every SECOND
    bool slow = false;

    void update(float dt) override {
        if (isKeyPressed(Key::F)) {
            slow = !slow;
            setTargetFPS(slow ? 20 : 60);
        }
        redX = wrap(redX + 5, 0, 800);            // forgot dt!
        greenX = wrap(greenX + 300 * dt, 0, 800); // uses dt
    }

    void draw() override {
        drawRect(redX, 150, 40, 40, Color::Red);
        drawRect(greenX, 280, 40, 40, Color::Green);
    }

    void drawUI() override {
        drawText(slow ? "20 frames per second (press F)" : "60 frames per second (press F)", 10, 10, 20);
        drawText("Red: x + 5 every frame", 10, 120, 20, Color::Red);
        drawText("Green: x + 300 * dt every frame", 10, 250, 20, Color::Green);
        drawFPS(700, 10);
    }
};

int main() {
    Game game("Delta time demo", 800, 450);
    game.run<DeltaTimeDemo>();
}
```

At 20 frames per second the red square crawls at a third of its speed, while
the green one doesn't care. (`wrap(value, 0, 800)` makes a square that goes off
the right edge come back on the left. `slow ? 20 : 60` means "20 if `slow` is
true, otherwise 60".)

### Time functions

You can ask about time from anywhere:

| Function | What it gives you |
|---|---|
| `deltaTime()` | The same as `dt`: seconds since the last frame |
| `timeSinceStart()` | Seconds since the game started |
| `fps()` | Frames per second right now |
| `setTargetFPS(30)` | How many frames per second the game tries to run at (60 by default) |
| `drawFPS(x, y)` | Draws the frames-per-second counter (handy while testing) |

For waiting ("do this in 2 seconds", "shoot at most 4 times a second") use the
ready-made [timers](11-timers.md) instead of counting `dt` yourself.

---

## Coordinates: where things are

Positions in Sprout are measured in **pixels** (the tiny dots that make up the
screen). The point `(0, 0)` is the **top-left corner** of the window. **x** gets
bigger going **right** and **y** gets bigger going **down**:

```
(0,0) ────────────────────────────► x     (800,0)
  │
  │        ● (200,100)
  │
  │                    ● (400,225)  the middle of an 800x450 window
  │
  ▼ y
(0,450)                                   (800,450)
```

"Y goes down" surprises people who remember graphs from math class, where y
goes up. Just remember:

- to move **up**, make y **smaller** (`position.y -= 100 * dt;`)
- to move **down**, make y **bigger**
- **gravity** is a **positive** number (it pulls y bigger)
- **jumping** sets the y speed to a **negative** number

### Middle or corner?

This is the most common mix-up, so it's worth learning now:

| Thing | Its x and y are... |
|---|---|
| An entity's `position` | its **middle** |
| A `Sprite`'s `position` | its **middle** (by default) |
| `drawCircle(center, ...)` | the **middle** of the circle |
| `drawTextCentered(text, center, ...)` | the **middle** of the text |
| A `Rect{x, y, width, height}` | its **top-left corner** |
| `drawRect(x, y, w, h, ...)` | the **top-left corner** |
| `drawText(text, x, y, ...)` | the **top-left corner** of the text |
| `drawTexture(texture, position)` | the **top-left corner** of the picture |

Need a rectangle around a middle point? Use `Rect::fromCenter(middle, size)`. An
entity's `bounds()` does exactly that with its `position` and `size`.

### Angles

Angles are always in **degrees** (a full turn is 360). **0 points right** and
**90 points down** (because y goes down), so positive angles turn **clockwise**
on the screen:

```
            270 (up)
               ▲
               │
180 (left) ◄───┼───► 0 (right)
               │
               ▼
            90 (down)
```

### World and screen

There are two kinds of coordinates:

- **World coordinates:** where things are in your game world. Entities, tile
  maps and everything you draw in `draw()` use these.
- **Screen coordinates:** where things are on the window. `drawUI()`, the mouse
  position and UI buttons use these.

When the [camera](10-camera-and-effects.md) hasn't moved, the two are the same,
so you don't need to think about it. Once the camera follows a player around a
big level, they're different. That's why the score goes in `drawUI()`: it stays
put on the screen while the world scrolls.

---

## Everything that happens in one frame

Here's the complete, exact order. You don't need to memorise it, but it will
help when something surprising happens.

1. **Time:** `dt` is measured (and capped at 0.1 seconds).
2. **Input:** the keyboard, mouse, gamepads and actions are read. Music keeps
   streaming.
3. **Your scene's `update(dt)`** runs.
4. If the scene is **not paused**:
   1. timers set up with `after(...)` and `every(...)` that are due run,
   2. every entity's `update(dt)` runs, in the order they were spawned,
   3. particles move.
5. Entities that were **destroyed** are removed for good.
6. The **camera** updates (following limits, screen shake).
7. **Drawing:**
   1. the screen is wiped with your scene's `background` color,
   2. through the camera: your scene's `draw()`, then every visible entity's
      `draw()` (lowest `layer` first), then the particles,
   3. your scene's `drawUI()`, on top of everything, without the camera.
8. The picture is shown on the screen, and Sprout waits a little to keep a
   steady 60 frames per second.
9. If you called `changeScene<...>()` or `restartScene()` during this frame,
   the old scene is thrown away **now**, the new one is created and its
   `start()` runs.

A few things follow from that order:

- **An entity's `start()` runs immediately** when you call `spawn`, before
  `spawn` even returns.
- An entity spawned by your **scene** (in `start()`, in `update()` or in an
  `after`/`every` timer) gets its first `update()` in the **same** frame.
- An entity spawned by **another entity's** `update()` (like a bullet fired by
  the player) gets its first `update()` in the **next** frame. It is still drawn
  in the same frame, so you'll see it straight away.
- `destroy()` doesn't make an entity vanish instantly. It's marked as
  destroyed (so `findAll`, `findFirst` and `count` ignore it), and it's really
  removed at the end of the update part of the frame.
- `changeScene` and `quitGame` also wait until the end of the frame, so the code
  after them still runs. Add a `return;` after them if you want to stop right
  there.

### Before the first frame

When you create the `Game`:

```cpp
Game game("My Game", 800, 450);
```

Sprout opens the window, sets the target to 60 frames per second, starts the
sound system, and makes your game's own folder the **working folder**. That's
why `"assets/player.png"` is always found, no matter where you started the game
from. Then `game.run<FirstScene>()` creates your first scene, runs its `start()`
and begins the loop.

### Closing the game

The loop stops when:

- the player closes the window,
- the player presses **Esc** (you can change or turn off this key with
  `setExitKey(Key::None)`), or
- your code calls `quitGame()`.

If your game hits a C++ error while running (an "exception"), Sprout closes the
window and prints a readable message in the terminal instead of crashing
silently.

---

**Next:** [Scenes](04-scenes.md), the screens of your game.
