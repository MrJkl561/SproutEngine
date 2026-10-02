# 4. Scenes

A **scene** is one screen of your game. Most games have a few:

```
 ┌──────────────┐   Space   ┌──────────────┐  time's up  ┌──────────────┐
 │ Title screen │ ────────► │   The game   │ ──────────► │  Game over   │
 └──────────────┘           └──────────────┘             └──────┬───────┘
                                   ▲              Enter         │
                                   └────────────────────────────┘
```

Only one scene runs at a time. When you switch, the old scene is thrown away
(with all its entities and timers) and the new one starts fresh. It's like
changing the slide in a presentation.

---

## Making a scene

Make a struct that **inherits** from `Scene` (writes `: Scene` after its name)
and fill in the functions you need. You don't have to fill in all of them;
anything you leave out simply does nothing.

```cpp
struct Level1 : Scene {
    void start() override {
        // Runs once, when the scene begins. Set things up here.
    }

    void update(float dt) override {
        // Runs every frame. Move things, read input, check collisions.
    }

    void draw() override {
        // Runs every frame. Draw the game world (seen through the camera).
    }

    void drawUI() override {
        // Runs every frame, after draw(). Draw the score, menus...
        // (on top of everything, and the camera doesn't move it)
    }
};
```

> **Always write `override`.** It tells the compiler "I'm filling in one of
> Scene's functions". If you make a typo (`void updat(float dt) override`) or get
> the parameters wrong (`void update() override`), the compiler tells you,
> instead of your function silently never running.

> **Don't need `dt`?** You can leave out its name: `void update(float) override`.
> That stops some compilers from warning about an unused parameter.

To start the game with a scene, give it to `game.run`:

```cpp
int main() {
    Game game("My Game", 800, 450);
    game.run<Level1>();
}
```

## What every scene has

Every scene comes with these, ready to use:

| Member | What it is |
|---|---|
| `background` | The color the screen is wiped with every frame. Black unless you change it: `background = Color::SkyBlue;` |
| `camera` | The scene's camera. See [Camera and effects](10-camera-and-effects.md). |
| `particles` | The scene's particle effects. See [Camera and effects](10-camera-and-effects.md#particles). |
| `paused` | Set it to `true` to freeze the game. See [Pausing](#pausing). |

Scenes can also spawn and find entities (`spawn`, `findAll`, ...; see
[Entities](05-entities.md)), run timers (`after`, `every`; see
[Timers](11-timers.md)) and hold solid walls (`addSolid`; see
[Levels and physics](09-levels-and-physics.md)).

Your own variables go inside the struct too. They're reset every time the scene
starts:

```cpp
struct Level1 : Scene {
    int score = 0;
    int lives = 3;
    Vec2 playerPosition{400, 300};
    // ...
};
```

---

## Switching scenes

From anywhere in your game (a scene, an entity, a button...):

```cpp
changeScene<GameOver>();
```

The `<GameOver>` part says which scene to go to. The switch happens at the
**end of the current frame**, so the rest of your code in this frame still runs.
If you want to stop right away, add `return;` after it.

### Giving values to the next scene

Often the next scene needs to know something, such as the final score. Give the
scene a **constructor** that takes the value, then pass it to `changeScene`:

```cpp
struct GameOver : Scene {
    int finalScore;

    GameOver(int score) {        // the constructor: runs when the scene is made
        finalScore = score;
    }

    void drawUI() override {
        drawTextCentered("Score: " + std::to_string(finalScore), screenCenter(), 40);
    }
};

// ...somewhere in your game scene:
changeScene<GameOver>(score);    // `score` goes into GameOver's constructor
```

You can pass several values, as long as they match the constructor:
`changeScene<Level>(2, "Ice Cave")` works with a constructor like
`Level(int number, std::string name)`. The values are **copied**, so it's fine
that the old scene (and its variables) is about to be thrown away.

`game.run` takes values the same way: `game.run<Level>(1);`.

### Starting again: `restartScene()`

```cpp
if (isKeyPressed(Key::R)) restartScene();
```

`restartScene()` throws the current scene away and makes a brand-new copy, as if
you had just arrived. It even gets the **same values** it was made with, so a
`Level` made with `changeScene<Level>(3)` restarts as level 3.

A level system can be this small:

```cpp
#include <sprout/sprout.hpp>

using namespace sprout;

struct Level : Scene {
    int number;

    Level(int levelNumber) {
        number = levelNumber;
    }

    void update(float) override {
        if (isKeyPressed(Key::N)) changeScene<Level>(number + 1); // next level
        if (isKeyPressed(Key::R)) restartScene();                 // same level again
    }

    void drawUI() override {
        drawTextCentered("Level " + std::to_string(number), screenCenter(), 50);
    }
};

int main() {
    Game game("Levels", 800, 450);
    game.run<Level>(1); // start at level 1
}
```

### Quitting

```cpp
quitGame(); // closes the game at the end of this frame
```

Handy for a "Quit" button on your title screen. (Players can also close the
window or press Esc.)

### Which scene is running?

`currentScene()` gives you the scene that's running right now. You'll rarely
need it, because inside a scene you *are* the scene, and inside an entity you
can use `scene()`.

---

## A complete example: title, game and game over

This game has three scenes. Collect as many coins as you can in 10 seconds.

```cpp
#include <sprout/sprout.hpp>

using namespace sprout;

// --- The game itself ----------------------------------------------------------
struct PlayScene : Scene {
    int score = 0;
    Vec2 player{400, 225};
    Vec2 coin{200, 150};
    Timer clock{10.0f}; // you have 10 seconds

    void start() override {
        background = Color::DarkGreen;
    }

    void update(float dt) override; // written further down - see below!

    void draw() override {
        drawCircle(coin, 10, Color::Gold);
        drawCircle(player, 20, Color::SkyBlue);
    }

    void drawUI() override {
        drawText("Score: " + std::to_string(score), 10, 10, 30);
        drawText("Time: " + std::to_string(int(clock.timeLeft())), 10, 45, 30);
        if (paused) drawTextCentered("PAUSED", screenCenter(), 60, Color::Yellow);
    }
};

// --- Game over: shows the score it was given ---------------------------------------
struct GameOverScene : Scene {
    int finalScore;

    GameOverScene(int score) {
        finalScore = score;
    }

    void update(float) override {
        if (isKeyPressed(Key::Enter)) changeScene<PlayScene>();
    }

    void drawUI() override {
        drawTextCentered("Time's up!", {400, 150}, 60, Color::Gold);
        drawTextCentered("You got " + std::to_string(finalScore) + " coins", {400, 230}, 30);
        drawTextCentered("Press Enter to play again", {400, 300}, 20, Color::LightGray);
    }
};

// --- The title screen -------------------------------------------------------------
struct TitleScene : Scene {
    void start() override {
        background = Color::DarkBlue;
    }

    void update(float) override {
        if (isKeyPressed(Key::Space)) changeScene<PlayScene>();
    }

    void drawUI() override {
        drawTextCentered("COIN RUSH", {400, 170}, 70, Color::Gold);
        drawTextCentered("Press Space to start", {400, 260}, 24);
    }
};

// PlayScene's update() lives down here because it needs GameOverScene,
// which C++ only knows about after the lines above.
void PlayScene::update(float dt) {
    if (isActionPressed("pause")) paused = !paused;
    if (paused) return;

    player += getVector("left", "right", "up", "down") * 300 * dt;

    if (player.distanceTo(coin) < 30) {
        score += 1;
        coin = randomPointIn(Rect{40, 40, 720, 370});
    }

    if (isKeyPressed(Key::R)) restartScene();
    if (clock.isDone()) changeScene<GameOverScene>(score);
}

int main() {
    Game game("Coin Rush", 800, 450);
    game.run<TitleScene>();
}
```

A few new things in there:

- `Timer clock{10.0f};` is a timer that is done after 10 seconds. It starts when
  the scene is made. See [Timers](11-timers.md).
- `int(clock.timeLeft())` turns the time left (like `7.36`) into a whole number
  (`7`) by chopping off the decimals.
- `player.distanceTo(coin)` is the distance in pixels between two points.
- `randomPointIn(rect)` picks a random point inside a rectangle.

### When two scenes need each other

This is the most common C++ puzzle when making a game with several scenes, so
let's look at it closely.

C++ reads your file **from top to bottom**, and you can only use a struct after
it has been written. Now imagine a title screen that switches to the level, and
a level that switches back to the title:

```cpp
struct Title : Scene {
    void update(float) override {
        if (isKeyPressed(Key::Space)) changeScene<Level>(); // Level isn't written yet!
    }
};

struct Level : Scene {
    void update(float) override {
        if (isKeyPressed(Key::Backspace)) changeScene<Title>();
    }
};
```

The compiler stops at the first `changeScene<Level>()` with an error like:

```
error: 'Level' was not declared in this scope            (GCC)
error: use of undeclared identifier 'Level'              (Clang, Xcode)
error C2065: 'Level': undeclared identifier              (Visual Studio)
```

Swapping the two structs doesn't help, because then `Level` would use `Title`
before *it* is written. The fix is to split one function into two parts:

1. **Inside** the struct, only *announce* the function: end it with `;` instead
   of `{ ... }`.
2. **Below both scenes**, write the function's body. Put the struct's name and
   `::` in front of the function name, so C++ knows whose function it is (and
   leave out `override` there).

Here's the complete, working version:

```cpp
#include <sprout/sprout.hpp>

using namespace sprout;

struct Title : Scene {
    void update(float dt) override; // just announced here - the body is further down
    void drawUI() override {
        drawTextCentered("Press Space to play", screenCenter(), 30);
    }
};

struct Level : Scene {
    void update(float) override {
        if (isKeyPressed(Key::Backspace)) changeScene<Title>(); // fine: Title is above
    }
    void drawUI() override {
        drawTextCentered("Playing! Backspace goes back", screenCenter(), 30);
    }
};

// Now that both scenes exist, Title's update can switch to Level.
void Title::update(float) {
    if (isKeyPressed(Key::Space)) changeScene<Level>();
}

int main() {
    Game game("Two scenes", 800, 450);
    game.run<Title>();
}
```

The Coin Rush example above uses the same trick for `PlayScene::update`, and so
do `examples/05_coin_collector`, `examples/07_platformer` and
`examples/09_menus_and_settings`. It works for any function, in scenes or
entities.

> **Bigger games:** once your `main.cpp` gets long, you can split it into several
> files (`player.hpp`, `enemies.hpp`, ...). Every `.cpp` file in your game's folder
> is built automatically. But for your first few games, one file is perfectly
> fine.

---

## Pausing

Every scene has a `bool paused`. While it's `true`:

- entities stop updating (they freeze in place, but are still drawn),
- particles freeze,
- `after(...)` and `every(...)` timers stop counting,
- `Timer` and `Cooldown` stop counting,
- but **your scene's own `update()` keeps running**, so you can un-pause.

The usual way to use it:

```cpp
void update(float dt) override {
    if (isActionPressed("pause")) paused = !paused; // P key or gamepad Start
    if (paused) return;                              // skip the rest while paused

    // ...the normal game code...
}

void drawUI() override {
    if (paused) drawTextCentered("PAUSED", screenCenter(), 60);
}
```

`paused = !paused` flips it: `!` means "not", so true becomes false and false
becomes true. The `"pause"` action already exists and is bound to **P** and the
gamepad's **Start** button. Sprout doesn't pause by itself when you press P;
you decide what happens.

---

## Keeping things between scenes

When a scene is thrown away, so are its variables. To keep something for later:

- **pass it to the next scene's constructor** (like `finalScore` above). This is
  best for things like "the score of this round" or "which level is next".
- **save it** in a [save file](13-saving.md). This is best for high scores,
  settings and unlocked levels, which should survive closing the game.
- for small games, a **variable outside of any struct** (at the top of your file)
  works too, like `int totalCoins = 0;`. It lives for as long as the game runs.

---

## Quick reference

| Code | What it does |
|---|---|
| `struct Name : Scene { ... };` | Makes a scene |
| `void start() override` | Runs once at the beginning |
| `void update(float dt) override` | Runs every frame |
| `void draw() override` | Draws the world (through the camera) |
| `void drawUI() override` | Draws on top, without the camera |
| `game.run<Name>(values...)` | Starts the game with this scene |
| `changeScene<Name>(values...)` | Switches scene at the end of the frame |
| `restartScene()` | Starts the current scene again with the same values |
| `quitGame()` | Closes the game at the end of the frame |
| `currentScene()` | The scene that is running |
| `background`, `camera`, `particles`, `paused` | Things every scene has |

**Next:** fill your scenes with [Entities](05-entities.md).
