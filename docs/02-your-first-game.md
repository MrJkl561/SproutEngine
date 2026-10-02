# 2. Your first game: Catch the Falling Stars

In this tutorial you'll build a complete little game from an empty file.
Stars fall from the night sky. You move a basket to catch them. Every star you
catch is a point. Every star you miss costs a life. Lose three lives and it's
game over, and then you press Enter to try again.

```
   Score: 7                              *
   Lives: 2
                    *
                                  *

                         \_________/        <- your basket
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ (grass)
```

It's about 120 lines of code, and you'll write every one of them yourself, a
few at a time. After each step you can build the game and see what changed.

**What you'll learn:** scenes, entities, moving things smoothly, keyboard
input, collisions, timers, random numbers, drawing text and shapes, particles,
screen shake and restarting the game.

> **Before you start:** you need the tools installed and Sprout built once.
> If you haven't done that yet, follow [Installing and building](01-installing.md)
> first. It's fine if you only know a little C++. Every new idea is explained the
> first time it shows up.

---

## Step 0: Make a folder for your game

Every game lives in its own folder inside `games/`. Sprout finds those folders
by itself, so you don't have to change any build files.

1. Go to the `games` folder inside the Sprout folder.
2. Make a new folder called `catch_the_stars`.
3. Inside it, make an empty text file called `main.cpp`.

You should now have:

```
games/
├── my_first_game/
│   └── main.cpp
└── catch_the_stars/      <- new!
    └── main.cpp          <- new, still empty
```

> **Folder names:** use lowercase letters, numbers and `_` only. The folder name
> becomes the name of your game's program.

To build your game, open a terminal in the Sprout folder and run:

```bash
cmake -S . -B build
cmake --build build --target catch_the_stars
```

The first command tells CMake (the build tool) to look for new games. The second
one **compiles** your game. Compiling means turning your C++ text into a program
the computer can run. `--target catch_the_stars` means "only build this one
game", which is faster than building everything.

Then run your game:

| System | Command |
|---|---|
| Windows | `build\bin\catch_the_stars\catch_the_stars.exe` |
| macOS / Linux | `./build/bin/catch_the_stars/catch_the_stars` |

(Using Visual Studio, VS Code or CLion instead of a terminal? See
[Installing and building](01-installing.md#building-from-a-code-editor). Pick
`catch_the_stars` as the thing to run.)

Your `main.cpp` is still empty, so the build will fail right now. Let's fix that.

---

## Step 1: A night sky and some grass

Type this into `main.cpp`. Typing it yourself (instead of copying it) helps you
remember it, but copying is fine too.

```cpp
#include <sprout/sprout.hpp>

using namespace sprout;

struct PlayScene : Scene {
    void start() override {
        background = Color{20, 20, 50}; // a dark night sky
    }

    void draw() override {
        drawRect(0, 430, 800, 20, Color::DarkGreen); // grass
    }
};

int main() {
    Game game("Catch the Falling Stars", 800, 450);
    game.run<PlayScene>();
}
```

Build it and run it. You should see a dark blue window with a strip of green at
the bottom. Press **Esc** (or click the window's close button) to quit.

Now let's go through it line by line.

```cpp
#include <sprout/sprout.hpp>
```

This brings in the whole Sprout engine, like opening a toolbox so you can use
the tools inside.

```cpp
using namespace sprout;
```

Everything in Sprout lives in a **namespace** (a named group) called `sprout`.
Without this line you'd have to write `sprout::Color`, `sprout::Scene` and so on
everywhere. With it, you can just write `Color` and `Scene`.

```cpp
struct PlayScene : Scene {
```

This makes a new **class** (a `struct` is a class) called `PlayScene`. A class
is a blueprint: it describes what something has and what it can do.

The `: Scene` part is called **inheritance**. It means "a PlayScene is a kind of
Scene". Our PlayScene gets everything a Sprout scene already knows how to do,
and we only add the parts that make our game special.

A **scene** is one screen of your game, such as a title menu, a level or a
game-over screen. Our game only needs one.

```cpp
    void start() override {
        background = Color{20, 20, 50}; // a dark night sky
    }
```

`start()` is a **function** (a named block of code). Sprout runs a scene's
`start()` once, when the scene begins. It's the place to set things up.

`override` means "I'm filling in a blank that Scene left for me". Scene already
has an empty `start()`, and ours replaces it. If you make a typo like
`void strat() override`, the compiler will complain, which is exactly what you
want. Without `override` your typo would silently never run!

`background` is the color the screen is wiped with every frame.
`Color{20, 20, 50}` mixes a color from **red, green and blue**, each from 0 to
255. A little red, a little green and a bit more blue gives a dark night sky.

Everything after `//` is a **comment**: a note for humans that the compiler
ignores.

```cpp
    void draw() override {
        drawRect(0, 430, 800, 20, Color::DarkGreen); // grass
    }
```

Sprout calls `draw()` every **frame**. A game is like a flip book: it draws a
picture, then another slightly different one, then another, about 60 times a
second. Each picture is one frame.

`drawRect(x, y, width, height, color)` draws a filled rectangle. `x` and `y` are
its **top-left corner**. In games, positions are measured in pixels from the
top-left of the window, and **y gets bigger going down**:

```
(0,0) ──────────────► x        (800,0)
  │
  │      (400,225) is the middle
  │
  ▼ y
(0,450)                        (800,450)
```

So our grass starts at the left edge (x = 0), 430 pixels down. It's 800 pixels
wide (the whole window) and 20 pixels tall.

```cpp
int main() {
    Game game("Catch the Falling Stars", 800, 450);
    game.run<PlayScene>();
}
```

Every C++ program starts at `main()`. The first line opens a window with a title
that is 800 pixels wide and 450 pixels tall. The second line starts our
`PlayScene` and keeps the game running until the window is closed. The
`<PlayScene>` part tells `run` *which* scene to start with.

> **Try it!** Change the background to `Color::Black`, or to
> `Color{80, 0, 120}`. Make the grass taller. (Remember: if the grass gets taller,
> its `y` has to get smaller, or it will hang off the bottom of the window.)

---

## Step 2: The basket

Now let's add the basket. The basket is an **entity**. An entity is a "thing" in
your game: the player, an enemy, a coin, a bullet. Entities draw themselves,
move themselves and can bump into each other.

Add this **above** `struct PlayScene` (C++ reads from top to bottom, so things
must be written before they are used):

```cpp
struct Basket : Entity {
    Basket() {
        position = {400, 405};
        size = {100, 24};
    }

    void draw() override {
        drawRoundedRect(bounds(), 8, Color::Brown);
        drawRect(bounds().x, bounds().y, size.x, 6, Color::Beige); // the rim
    }
};
```

And add one line to the scene's `start()`:

```cpp
    void start() override {
        background = Color{20, 20, 50}; // a dark night sky
        spawn<Basket>();
    }
```

Build and run. There's a basket sitting on the grass!

What's new here:

- `struct Basket : Entity` is a new kind of entity. Like before, `: Entity`
  means it gets everything an entity already has.
- `Basket() { ... }` is a **constructor**. It has the same name as the struct
  and runs when a basket is created. We use it to set up the basket.
- Every entity has a `position` and a `size`. **An entity's position is its
  middle**, not its corner. The `size` is the size of its **hitbox**, the
  invisible box used to check if things touch. Our basket is 100 pixels wide and
  24 tall, and its middle is at x = 400 (the middle of the window), y = 405.
- `{400, 405}` makes a **Vec2**: a pair of numbers, `x` and `y`. Sprout uses
  `Vec2` for positions, sizes, speeds and directions.
- `bounds()` gives you the hitbox as a **Rect** (a rectangle). A `Rect` has
  `x`, `y`, `width` and `height`, and its `x` and `y` are the **top-left
  corner**. That's handy for drawing, because `drawRoundedRect` wants a Rect.
- `drawRoundedRect(rect, 8, color)` draws a rectangle with corners rounded by 8
  pixels. Then we draw a thin beige strip along the top as the rim.
- `spawn<Basket>()` creates a basket and adds it to the scene. From now on the
  scene draws it every frame for us.

Here's the whole file so far, so you can check yours:

```cpp
#include <sprout/sprout.hpp>

using namespace sprout;

struct Basket : Entity {
    Basket() {
        position = {400, 405};
        size = {100, 24};
    }

    void draw() override {
        drawRoundedRect(bounds(), 8, Color::Brown);
        drawRect(bounds().x, bounds().y, size.x, 6, Color::Beige); // the rim
    }
};

struct PlayScene : Scene {
    void start() override {
        background = Color{20, 20, 50}; // a dark night sky
        spawn<Basket>();
    }

    void draw() override {
        drawRect(0, 430, 800, 20, Color::DarkGreen); // grass
    }
};

int main() {
    Game game("Catch the Falling Stars", 800, 450);
    game.run<PlayScene>();
}
```

---

## Step 3: Make it move

A basket that can't move isn't much fun. Add a `speed` and an `update()`
function to the Basket:

```cpp
struct Basket : Entity {
    float speed = 450; // pixels per second

    Basket() {
        position = {400, 405};
        size = {100, 24};
    }

    void update(float dt) override {
        position.x += getAxis("left", "right") * speed * dt;
    }

    void draw() override {
        drawRoundedRect(bounds(), 8, Color::Brown);
        drawRect(bounds().x, bounds().y, size.x, 6, Color::Beige); // the rim
    }
};
```

Build and run, then press the **left and right arrow keys** (or **A** and **D**).
The basket moves!

- `float speed = 450;` is a **member variable**: a value every basket carries
  around. `float` means a number that can have a decimal point, like `3.5`.
- `update(float dt)` runs **every frame**, before anything is drawn. It's where
  things move and decisions are made.
- `getAxis("left", "right")` gives you **-1** while "left" is held, **+1** while
  "right" is held, and **0** when neither (or both) are held. `"left"` and
  `"right"` are ready-made **actions** that already work with the arrow keys,
  A/D and a gamepad. (See [Input](06-input.md) for more.)
- `position.x += ...` means "add this much to the basket's x".

### What is `dt`?

`dt` stands for **delta time**: the number of seconds since the last frame. At
60 frames per second, `dt` is about `0.0167` (one sixtieth of a second).

Why multiply by it? Because not every computer runs at the same speed. Let's
do the math for one frame of holding the right arrow:

```
getAxis = 1,  speed = 450,  dt = 0.0167
1 * 450 * 0.0167 = about 7.5 pixels this frame
```

60 frames of 7.5 pixels is 450 pixels in one second, which is exactly our speed.
On a slower computer running at 30 frames per second, `dt` is twice as big, so
the basket moves 15 pixels per frame, which is *still* 450 pixels per second.
**Multiplying by `dt` makes your game run at the same speed on every computer.**
Use it for anything that moves or changes over time.

---

## Step 4: Stay on the screen

Hold the left arrow for a while... the basket slides right off the screen! Add
one more line to the Basket's `update()`:

```cpp
    void update(float dt) override {
        position.x += getAxis("left", "right") * speed * dt;

        // Don't let the basket leave the screen
        position.x = clamp(position.x, size.x / 2, screenWidth() - size.x / 2);
    }
```

`clamp(value, smallest, biggest)` keeps a value between two limits. If the
value is too small you get `smallest`, and if it's too big you get `biggest`.
`screenWidth()` is the width of the window (800).

Why `size.x / 2`? Remember, `position` is the **middle** of the basket. When the
basket touches the left edge, its middle is half a basket (50 pixels) away from
the edge.

---

## Step 5: Falling stars

Time for the stars. Add this new entity **below the Basket** and **above the
PlayScene**:

```cpp
struct Star : Entity {
    float fallSpeed;

    Star(float x, float speed) {
        position = {x, -20}; // start just above the top of the screen
        size = {28, 28};
        fallSpeed = speed;
    }

    void update(float dt) override {
        position.y += fallSpeed * dt;
    }

    void draw() override {
        drawCircle(position, 18, Color::Gold.withAlpha(0.3f)); // a soft glow
        drawCircle(position, 12, Color::Gold);
        drawCircle(position, 5, Color::White);
    }
};
```

This time the constructor takes two **parameters**: `x` (where across the screen
the star starts) and `speed` (how fast it falls). We'll give it these values
when we spawn it.

`position.y += fallSpeed * dt;` moves the star **down** a little every frame
(remember, y gets bigger going down).

`drawCircle(center, radius, color)` draws a circle around its **middle**, which
is perfect, because `position` is the middle. We draw three circles on top of
each other: a big see-through one for a glow, a gold one and a small white
center. `Color::Gold.withAlpha(0.3f)` is gold that is only 30% solid (`0.0f` is
invisible and `1.0f` is solid). The `f` at the end of `0.3f` just tells C++
"this is a float".

Now make the scene drop stars. Update `PlayScene` like this:

```cpp
struct PlayScene : Scene {
    float starSpeed = 150;

    void start() override {
        background = Color{20, 20, 50}; // a dark night sky
        spawn<Basket>();

        // Every 0.8 seconds, drop a new star somewhere along the top
        every(0.8f, [this] {
            spawn<Star>(randomFloat(30, 770), starSpeed);
        });
    }

    void draw() override {
        drawRect(0, 430, 800, 20, Color::DarkGreen); // grass
    }
};
```

Build and run. It's raining stars!

There's a lot packed into those few lines, so let's slow down:

- `every(0.8f, ...)` means "do this again and again, every 0.8 seconds".
- The part in the middle, `[this] { ... }`, is called a **lambda**. A lambda is
  a little packet of code that you hand over to be run *later*. Here we hand it
  to `every`, and `every` runs it every 0.8 seconds.
- The `[this]` at the start lets the code inside use the scene's own things,
  like `spawn` and `starSpeed`. If you forget it, the compiler will say
  something about `this` not being captured.
- `randomFloat(30, 770)` picks a random number between 30 and 770, so each star
  appears somewhere different along the top.
- `spawn<Star>(randomFloat(30, 770), starSpeed)` creates a star. The values in
  the brackets are handed to the Star's constructor, so they become its `x` and
  its `speed`.

> **Try it!** Change `0.8f` to `0.2f`. Now try `3.0f`. Try making the stars
> bigger or giving them a different color.

---

## Step 6: Catching stars

Stars fall straight through the basket. Let's catch them! Add a `score` to the
scene and an `update()` function that checks every star:

```cpp
struct PlayScene : Scene {
    int score = 0;
    float starSpeed = 150;

    void start() override {
        // ... same as before ...
    }

    void update(float dt) override {
        Basket* basket = findFirst<Basket>();
        for (Star* star : findAll<Star>()) {
            if (star->overlaps(*basket)) {
                // Caught it!
                star->destroy();
                score += 1;
                particles.burst(star->position, Color::Gold);
            }
        }
    }

    // ... draw() same as before ...
};
```

`int` means a whole number, like `0`, `1` or `-5`. Scores are whole numbers.

Line by line:

- `findFirst<Basket>()` finds the basket in the scene. It gives us a
  **pointer**, written `Basket*`. A pointer is like an address: it tells you
  where the basket lives, so you can reach it. To use something through a
  pointer you write `->` instead of `.`, as in `star->position`.
- `findAll<Star>()` gives us a list of every star in the scene, and
  `for (Star* star : ...)` runs the code inside once for each star.
- `star->overlaps(*basket)` is **true** if the star's hitbox touches the
  basket's hitbox. (The `*` in `*basket` turns the pointer back into the basket
  itself, because `overlaps` wants the basket, not its address.)
- `star->destroy()` removes the star from the scene.
- `score += 1` adds one point.
- `particles.burst(position, color)` makes a little explosion of sparkles. Every
  scene has a `particles` system built in.

Notice that the scene's `update()` doesn't use `dt` yet. That's fine. (We'll use
it in Step 10.) Some compilers print a **warning** like "unused parameter dt".
A warning is just a hint, not an error: your game still builds and runs.

---

## Step 7: Show the score

You're catching stars, but you can't see your score. Add a `drawUI()` function
to the scene:

```cpp
    void drawUI() override {
        drawText("Score: " + std::to_string(score), 10, 10, 30);
    }
```

- `drawUI()` is like `draw()`, but it's drawn **on top of everything** and is
  never moved by the camera. Use it for scores, health bars and menus. (In the
  next step the camera will shake, and you don't want your score to shake too.)
- `drawText(text, x, y, size)` writes text with its top-left corner at x, y.
  It's white unless you give it a color.
- `std::to_string(score)` turns the number into text, and `+` glues two pieces
  of text together. So when the score is 7 you get `"Score: 7"`.

---

## Step 8: Missing stars and losing lives

Now for the danger. Give the scene some lives:

```cpp
    int score = 0;
    int lives = 3;
    float starSpeed = 150;
```

Then, in the scene's `update()`, add an `else if` part after the "Caught it!"
part:

```cpp
        for (Star* star : findAll<Star>()) {
            if (star->overlaps(*basket)) {
                // Caught it!
                star->destroy();
                score += 1;
                particles.burst(star->position, Color::Gold);
            } else if (star->position.y > screenHeight() + 20) {
                // Missed it...
                star->destroy();
                lives -= 1;
                camera.shake(8, 0.3f);
            }
        }
```

`else if` means "otherwise, if...". If the star wasn't caught, we check whether
it fell below the bottom of the screen. `screenHeight()` is the window's height
(450), and we add 20 so the star has completely disappeared first. Then we
remove the star, take away a life and shake the camera by 8 pixels for 0.3
seconds. Ouch!

Show the lives too, in `drawUI()`:

```cpp
    void drawUI() override {
        drawText("Score: " + std::to_string(score), 10, 10, 30);
        drawText("Lives: " + std::to_string(lives), 10, 45, 30, Color::Pink);
    }
```

---

## Step 9: Game over, and play again

Right now your lives just go below zero forever. Let's add a game over. Add one
more variable to the scene:

```cpp
    bool gameOver = false;
```

A `bool` is either `true` or `false`, like a light switch.

In the "Missed it" part, check if that was the last life:

```cpp
            } else if (star->position.y > screenHeight() + 20) {
                // Missed it...
                star->destroy();
                lives -= 1;
                camera.shake(8, 0.3f);
                if (lives <= 0) {
                    gameOver = true;
                    cancelAllTimers(); // stop dropping new stars
                }
            }
```

`cancelAllTimers()` stops the `every(...)` we set up in `start()`, so no more
stars appear.

Now add this to the very **top** of the scene's `update()`:

```cpp
    void update(float dt) override {
        if (gameOver) {
            if (isKeyPressed(Key::Enter)) restartScene();
            return; // skip the rest of update()
        }

        // ... the rest of update() ...
```

When the game is over we don't want to count stars any more. We only wait for
the player to press Enter. `isKeyPressed(Key::Enter)` is true only on the one
frame when Enter is pressed down. `restartScene()` throws the whole scene away
and starts a fresh copy, so the score, the lives and the stars are all reset.
`return` leaves the function straight away, skipping the rest of `update()`.

Finally, show a game-over message at the end of `drawUI()`:

```cpp
        if (gameOver) {
            drawRect(0, 0, 800, 450, Color::Black.withAlpha(0.6f));
            drawTextCentered("GAME OVER", {400, 170}, 70, Color::Gold);
            drawTextCentered("You caught " + std::to_string(score) + " stars", {400, 240}, 30);
            drawTextCentered("Press Enter to play again", {400, 290}, 24, Color::LightGray);
        }
```

The see-through black rectangle covers the whole window and darkens the game
behind the text. `drawTextCentered(text, middle, size, color)` puts the
**middle** of the text at a point, which makes centering titles easy.

---

## Step 10: Make it get harder

A good game gets harder the longer you survive. Add this line to the scene's
`update()`, just after the `if (gameOver) { ... }` part:

```cpp
        starSpeed += 5 * dt; // stars fall a little faster every second
```

Each second, `starSpeed` grows by 5. After a minute, new stars fall at 450
pixels per second instead of 150. Here `dt` makes sure "5 per second" really
means per second, whatever the frame rate.

**You did it. You made a game!**

---

## The complete code

Here's the finished `main.cpp`. If something doesn't work, compare it with
yours line by line.

```cpp
// =============================================================================
//  Catch the Falling Stars
//  The game from docs/02-your-first-game.md
// =============================================================================
#include <sprout/sprout.hpp>

using namespace sprout;

// The basket at the bottom of the screen. The player moves it.
struct Basket : Entity {
    float speed = 450; // pixels per second

    Basket() {
        position = {400, 405};
        size = {100, 24};
    }

    void update(float dt) override {
        // -1 when "left" is held, +1 when "right" is held, 0 otherwise
        position.x += getAxis("left", "right") * speed * dt;

        // Don't let the basket leave the screen
        position.x = clamp(position.x, size.x / 2, screenWidth() - size.x / 2);
    }

    void draw() override {
        drawRoundedRect(bounds(), 8, Color::Brown);
        drawRect(bounds().x, bounds().y, size.x, 6, Color::Beige); // the rim
    }
};

// A star that falls from the sky.
struct Star : Entity {
    float fallSpeed;

    Star(float x, float speed) {
        position = {x, -20}; // start just above the top of the screen
        size = {28, 28};
        fallSpeed = speed;
    }

    void update(float dt) override {
        position.y += fallSpeed * dt;
    }

    void draw() override {
        drawCircle(position, 18, Color::Gold.withAlpha(0.3f)); // a soft glow
        drawCircle(position, 12, Color::Gold);
        drawCircle(position, 5, Color::White);
    }
};

// The scene where the game happens.
struct PlayScene : Scene {
    int score = 0;
    int lives = 3;
    float starSpeed = 150;
    bool gameOver = false;

    void start() override {
        background = Color{20, 20, 50}; // a dark night sky
        spawn<Basket>();

        // Every 0.8 seconds, drop a new star somewhere along the top
        every(0.8f, [this] {
            spawn<Star>(randomFloat(30, 770), starSpeed);
        });
    }

    void update(float dt) override {
        if (gameOver) {
            if (isKeyPressed(Key::Enter)) restartScene();
            return; // skip the rest of update()
        }

        starSpeed += 5 * dt; // stars fall a little faster every second

        Basket* basket = findFirst<Basket>();
        for (Star* star : findAll<Star>()) {
            if (star->overlaps(*basket)) {
                // Caught it!
                star->destroy();
                score += 1;
                particles.burst(star->position, Color::Gold);
            } else if (star->position.y > screenHeight() + 20) {
                // Missed it...
                star->destroy();
                lives -= 1;
                camera.shake(8, 0.3f);
                if (lives <= 0) {
                    gameOver = true;
                    cancelAllTimers(); // stop dropping new stars
                }
            }
        }
    }

    void draw() override {
        drawRect(0, 430, 800, 20, Color::DarkGreen); // grass
    }

    void drawUI() override {
        drawText("Score: " + std::to_string(score), 10, 10, 30);
        drawText("Lives: " + std::to_string(lives), 10, 45, 30, Color::Pink);

        if (gameOver) {
            drawRect(0, 0, 800, 450, Color::Black.withAlpha(0.6f));
            drawTextCentered("GAME OVER", {400, 170}, 70, Color::Gold);
            drawTextCentered("You caught " + std::to_string(score) + " stars", {400, 240}, 30);
            drawTextCentered("Press Enter to play again", {400, 290}, 24, Color::LightGray);
        }
    }
};

int main() {
    Game game("Catch the Falling Stars", 800, 450);
    game.run<PlayScene>();
}
```

---

## What you learned

| Idea | What it means | Where it was |
|---|---|---|
| Scene | One screen of your game, with `start`, `update`, `draw`, `drawUI` | `PlayScene` |
| Entity | A thing in the game that moves and draws itself | `Basket`, `Star` |
| `position` and `size` | The **middle** of an entity and its hitbox size | Basket's constructor |
| `update(dt)` and `dt` | Runs every frame; multiply speeds by `dt` | Steps 3, 5 and 10 |
| Actions | `getAxis("left", "right")` works with arrows, WASD and gamepads | Step 3 |
| `spawn<T>(...)` | Creates an entity and hands values to its constructor | Steps 2 and 5 |
| `every(seconds, [this] { ... })` | Runs code again and again | Step 5 |
| `findFirst`, `findAll`, `overlaps` | Finding entities and checking if they touch | Step 6 |
| `destroy()` | Removes an entity | Step 6 |
| `drawUI()` and `drawText` | Text on top that the camera doesn't move | Step 7 |
| `particles.burst`, `camera.shake` | Juicy effects | Steps 6 and 8 |
| `restartScene()` | Start the scene again from scratch | Step 9 |

---

## Try it! Ideas to make it your own

Start with the easy ones and work your way down.

1. **Your own style.** Change the colors, the basket size and the speeds. Make
   the stars `Color::SkyBlue` snowflakes instead.
2. **Faster basket over time.** Make the basket's speed grow too. (Hint: the
   basket can grow its own `speed` in its own `update()`.)
3. **Golden stars.** Give `Star` a `bool golden` and use `chance(0.1f)` (true 10%
   of the time) when spawning to make some stars golden, worth 5 points.
4. **Bombs.** Make a new `Bomb` entity that falls like a star but costs a life if
   you **catch** it. Spawn it with its own `every(...)`, and check
   `findAll<Bomb>()` in `update()` just like the stars.
5. **High score.** Save the best score so it's still there tomorrow. See
   [Saving](13-saving.md). A `SaveFile` and two lines of code will do it.
6. **Sound.** Put a sound file in a new folder `games/catch_the_stars/assets/`
   and call `playSound("assets/catch.wav");` when you catch a star. See
   [Sound and music](08-sound-and-music.md).
7. **Pictures.** Replace the circles with a picture:
   `drawTextureCentered(loadTexture("assets/star.png"), position);`.
   See [Drawing](07-drawing.md).
8. **A title screen.** Make a `TitleScene` that says "Press Space to start" and
   switches to `PlayScene` with `changeScene<PlayScene>();`. See
   [Scenes](04-scenes.md).
9. **Pause.** Put these two lines at the top of the scene's `update()`:
   `if (isActionPressed("pause")) paused = !paused;` and `if (paused) return;`.
   Now P pauses and un-pauses the game. Can you show "PAUSED" in `drawUI()`
   while `paused` is true? See [Scenes](04-scenes.md#pausing).

**Next:** learn what really happens every frame in
[How a game works](03-how-a-game-works.md).
