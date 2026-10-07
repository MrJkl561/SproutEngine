# Sprout

**A friendly C++ toolkit for people making their very first 2D games.**

Sprout comes in **two parts**. Pick whichever suits the way you like to work:

| | **Sprout Engine** | **Sprout Editor** |
|---|---|---|
| **What it is** | A code-only game library | A visual editor, like a small [Godot](https://godotengine.org) |
| **How you make a game** | Write one C++ file, build it, play | Place nodes in a scene with the mouse, attach C++ scripts, press Play |
| **Where it lives** | [`engine/`](engine/) (namespace `sprout::`) | [`editor/`](editor/) (namespace `s2d::`) |
| **Your games live in** | `games/<name>/main.cpp` in this repository | Project folders anywhere (`~/SproutProjects/<name>/` by default) |
| **Good for** | Learning how games work from the ground up; games that are mostly code | Levels you want to *see* and drag around; tweaking values without recompiling |
| **Change code while it runs** | No, rebuild and restart | Yes, scripts hot reload while the game runs |
| **Exports to** | Windows, macOS, Linux (whatever you build on) | Windows (x64 and ARM64), Linux, macOS and the **web**, from an Export button |
| **Multiplayer** | No | Yes: host/join, synced nodes, RPCs, dedicated servers |
| **Start here** | [Your first game](docs/02-your-first-game.md) | [Your first game in the editor](docs/editor/getting-started.md) |

Both are built from this repository with one command, and both use
[raylib](https://www.raylib.com) underneath.

## Engine or Editor?

**They're separate.** The editor is *not* a front end for the engine: it has
its own small node system, modelled on Godot's, and a game made with one can't
be opened by the other. They share a repository, a build and a love of
beginners, not code.

- **Choose the Engine** if you're learning to program games, like having
  everything in front of you as code, or are following a C++ course. There's
  less to learn: a game is a class with `update()` and `draw()`.
- **Choose the Editor** if you think visually, already know Godot (or Unity),
  want to build levels by dragging things around, or want a web build to put
  on itch.io.
- **Not sure?** Start with the engine tutorial. Everything you learn there
  (frames, `dt`, velocity, collisions) carries straight over to the editor.

### A taste of the Engine

A complete game: move the circle with the arrow keys, and press Space for
fireworks.

```cpp
#include <sprout/sprout.hpp>

using namespace sprout;

struct Hello : Scene {
    Vec2 player{400, 225};

    void update(float dt) override {
        player += getVector("left", "right", "up", "down") * 300 * dt;
        if (isKeyPressed(Key::Space)) particles.burst(player, randomColor(), 40);
    }

    void draw() override {
        drawCircle(player, 30, Color::Gold);
    }

    void drawUI() override {
        drawText("Arrow keys to move, Space for fireworks", 10, 10, 20);
    }
};

int main() {
    Game game("Hello, Sprout!", 800, 450);
    game.run<Hello>();
}
```

### A taste of the Editor

You build the scene with the mouse (a `CharacterBody2D` with a `Sprite2D` and
a `CollisionShape2D` inside), then attach a script to it. This one makes it run
and jump; the three `EXPORT` values show up in the Inspector so you can tune
them without touching the code:

```cpp
#include "script.hpp"

using namespace s2d;

class Hero : public Script {
public:
    EXPORT(float, speed, 250);
    EXPORT(float, jumpSpeed, 500);
    EXPORT(float, gravity, 1200);

    void process(float delta) override {
        auto* body = self<CharacterBody2D>();
        if (!body) return;

        body->velocity.x = 0;
        if (IsKeyDown(KEY_LEFT)) body->velocity.x = -speed;
        if (IsKeyDown(KEY_RIGHT)) body->velocity.x = speed;

        body->velocity.y += gravity * delta;
        if (body->isOnFloor() && IsKeyPressed(KEY_SPACE)) body->velocity.y = -jumpSpeed;

        body->moveAndSlide();
    }
};

REGISTER_SCRIPT(Hero)
```

```
+--------------------------------------------------------------+
| Projects New Open Save Save As Export   Play  Play Scene  Stop|
+------------+------------------------------------+------------+
| Scene      |                                    | Inspector  |
| (node tree)|        2D viewport                 | (the       |
|            |  click, drag, rotate, zoom, pan    |  selected  |
|------------|                                    |  node's    |
| FileSystem +------------------------------------+  fields +  |
| assets,    |  Output                            |  script)   |
| scenes,    |                                    |            |
| scripts    |                                    |            |
+------------+------------------------------------+------------+
```

---

## Quick start

One build makes the engine, its examples **and** the editor. It runs on
**Windows, macOS and Linux** (x86-64 and ARM64), and CMake downloads every
library for you.

You need a C++ compiler, **CMake 3.25 or newer** and **Git**. If you don't have
them yet, the [installation guide](docs/01-installing.md) walks you through it
step by step for Windows, macOS and Linux.

**1. Get Sprout:**

```bash
git clone https://github.com/MrJkl561/Cpp-Game-engine-.git
cd Cpp-Game-engine-
```

**2. Build it:**

```bash
cmake -S . -B build
cmake --build build
```

The first build downloads raylib and friends and compiles them, so it takes a
few minutes. After that it's quick.

**3. Run something:**

| | Windows | macOS / Linux |
|---|---|---|
| **Engine:** your first game | `build\bin\my_first_game\my_first_game.exe` | `./build/bin/my_first_game/my_first_game` |
| **Engine:** an example | `build\bin\04_pong\04_pong.exe` | `./build/bin/04_pong/04_pong` |
| **Editor** | `build\bin\sprout_editor\sprout_editor.exe` | `./build/bin/sprout_editor/sprout_editor` |

Even easier: the [build scripts](#build-scripts) below do the building for you,
e.g. `scripts\run-editor.bat` (Windows) or `scripts/run-editor.sh` (macOS,
Linux) builds and opens the editor.

(On Windows, run these commands in the **Developer PowerShell for VS**. Prefer
Visual Studio, VS Code or CLion? See
[Building from a code editor](docs/01-installing.md#building-from-a-code-editor).)

### Build scripts

The [`scripts/`](scripts/) folder has ready-made scripts for each kind of build
(`.bat` for Windows, `.sh` for macOS and Linux). Run them from the repository
folder:

| What | Windows | macOS / Linux |
|---|---|---|
| Build everything | `scripts\build.bat` | `scripts/build.sh` |
| A Debug build | `scripts\build.bat debug` | `scripts/build.sh debug` |
| Build and open the editor | `scripts\run-editor.bat` | `scripts/run-editor.sh` |
| Run the unit tests | `scripts\test.bat` | `scripts/test.sh` |
| Test the editor's multiplayer | `scripts\test-multiplayer.bat` | `scripts/test-multiplayer.sh` |
| Export a game | `scripts\export.bat windows,web [project] [folder]` | `scripts/export.sh linux,web [project] [folder]` |
| Set up web exports (once) | `scripts\setup-web.bat [emsdk folder]` | `scripts/setup-web.sh [emsdk folder]` |

They build into `build-x64\` or `build-arm64\` on Windows (for this PC's own CPU;
add `x64` or `arm64` to choose) and `build-release/` or `build-debug/` on
macOS and Linux.

**ARM64:** Sprout builds and runs on ARM64 as well as x86-64: Windows on ARM,
ARM64 Linux (e.g. a Raspberry Pi 5) and Apple Silicon Macs. The editor's
Windows export makes an x64 `.exe` that runs on every Windows PC, and there's a
separate "Windows ARM64" export for native ARM64 games. Mac exports are
universal apps (Apple Silicon and Intel).

## Making games with the Engine

Each engine game is a folder in `games/` with a `main.cpp`.

1. Copy the folder `games/my_first_game` and give the copy a new name, like
   `games/space_cats`.
2. Build again (`cmake -S . -B build` then `cmake --build build`, or
   `scripts/build.sh`). Sprout finds the new folder automatically.
3. Run `build/bin/space_cats/space_cats`, and start changing `main.cpp`!

Put pictures and sounds in an `assets` folder next to your `main.cpp`
(`games/space_cats/assets/`). It's copied next to your game every time you
build, so `loadTexture("assets/cat.png")` just works.

New to all this? Follow the tutorial **[Your first game](docs/02-your-first-game.md)**:
you'll build "Catch the Falling Stars" from an empty file, one step at a time.

### What the Engine gives you

- **Scenes** for each screen of your game (title, levels, game over), with
  `start`, `update`, `draw` and `drawUI`, and `changeScene<GameOver>(score)` to
  switch between them.
- **Entities** for the things in your game: `spawn<Enemy>(position)`,
  `findAll<Coin>()`, `findClosest<Enemy>(point)`, `destroy()` and easy collision
  checks.
- **Input actions** that work with keyboard, mouse and gamepads at once:
  `getVector("left", "right", "up", "down")`, `isActionPressed("jump")`.
- **Drawing** shapes, text, custom fonts, pictures, and **sprites** with sprite
  sheet animation, flipping and tinting.
- **Levels made of letters** with `TileMap`, and **`moveAndSlide`** physics for
  walls, floors, gravity and jumping.
- **Camera** that follows, zooms, stays inside the level and shakes.
- **Particles** for explosions, sparks, flames and confetti.
- **Sound effects and music** in one line: `playSound("assets/jump.wav")`.
- **Timers and cooldowns**: `every(2.0f, [this] { spawn<Enemy>(); })`,
  `Cooldown gun{0.25f}`.
- **Menus**: buttons, sliders, checkboxes, text boxes and themes.
- **Save files** for high scores and settings, and **JSON** for your own level
  and enemy data.
- `Vec2`, `Rect`, `clamp`, `lerp`, `moveToward`, angles in degrees, and easy
  **random numbers**.
- **Friendly messages** instead of crashes when a file is missing or something
  goes wrong.

**What the Engine isn't:** it's for 2D desktop games written in code. There's
no 3D, no networking, and no web or mobile export (the Editor can export to the
web). If you outgrow it, you can call
[raylib-cpp](https://github.com/RobLoach/raylib-cpp) directly from a Sprout
game, see
[Using raylib-cpp](docs/using-sprout-in-your-own-project.md#going-further-using-raylib-cpp-directly).

## Making games with the Editor

Open the editor (`scripts/run-editor.sh`, `scripts\run-editor.bat`, or the
`sprout_editor` program from the table above). The first time, it opens a demo
platformer: press **Play** (F5) to try it, **Stop** (F8 or Esc) to go back.

Then press **Projects** → **New Project...** to start your own game. Each
project is a folder of its own, anywhere you like, and the editor never needs
rebuilding for a new one:

```
My Game/
├── project.json    <- name, main scene, window size, export settings
├── scenes/         <- your scenes (*.scene.json)
├── assets/         <- pictures
├── scripts/        <- your C++ scripts
└── exports/        <- finished games, made by Export
```

New to all this? Follow **[Your first game in the editor](docs/editor/getting-started.md)**:
you'll make a character that runs and jumps, in about 15 minutes.

### What the Editor gives you

- **Godot-style nodes**: `Node2D`, `Sprite2D`, `AnimatedSprite2D`, `ColorRect`,
  `Label`, `Camera2D`, `StaticBody2D`, `CharacterBody2D` (with
  `moveAndSlide()`), `Area2D` and `CollisionShape2D`.
- **A live viewport**: drag, rotate, snap, zoom and pan; drop pictures in
  from your desktop.
- **C++ scripts on nodes** with `ready()` and `process(delta)`, which **hot
  reload** while the game runs.
- **`EXPORT(float, speed, 250);`** puts a script variable in the Inspector, so
  every node can have its own value.
- **A built-in script editor** with autocomplete, and **errors as you type**
  from [clangd](https://clangd.llvm.org/) (downloaded for you; nothing to
  install). Or open scripts in VS Code, Visual Studio, CLion...
- **Scenes inside scenes** (build the player once, use it in every level),
  and changing scenes while the game runs.
- **Sprite sheet animation** with named animations.
- **Export** to Windows (x64 and ARM64), Linux, macOS (universal) and the
  **web**, with one button.
- **Multiplayer** like Godot's: host or join a game, `MultiplayerSynchronizer`
  and `MultiplayerSpawner` nodes, `RPC`s, **Run Instances** to test several
  copies at once, and dedicated servers (`--server`). Other players move
  smoothly (interpolation), your own moves at once while the server stays in
  charge (client-side prediction), and shots hit what you saw (lag
  compensation). Start from the
  **Multiplayer** project template.
- **Several projects**, a Project Manager, undo/redo, and editable themes and
  panel sizes.

The whole manual is **[editor/README.md](editor/README.md)**.

---

## Learn

The **[documentation](docs/README.md)** is written for beginners.

### Learning the Editor

1. [Installing and building](docs/01-installing.md), if you don't have a C++
   compiler, CMake and Git yet (the same for both parts)
2. [Your first game in the editor](docs/editor/getting-started.md), a step-by-step tutorial
3. [Multiplayer in the editor](docs/editor/multiplayer.md): playing together over a network
4. The [Sprout Editor manual](editor/README.md): every dock, node, shortcut,
   the script API, scenes inside scenes, animation, exporting and themes

### Learning the Engine

Start at the top and work your way down:

1. [Installing and building](docs/01-installing.md)
2. [Your first game](docs/02-your-first-game.md), a step-by-step tutorial
3. [How a game works](docs/03-how-a-game-works.md): frames, `dt` and coordinates
4. [Scenes](docs/04-scenes.md)
5. [Entities](docs/05-entities.md)
6. [Input](docs/06-input.md)
7. [Drawing](docs/07-drawing.md)
8. [Sound and music](docs/08-sound-and-music.md)
9. [Levels and physics](docs/09-levels-and-physics.md)
10. [Camera and effects](docs/10-camera-and-effects.md)
11. [Timers](docs/11-timers.md)
12. [Menus and UI](docs/12-menus-and-ui.md)
13. [Saving](docs/13-saving.md)
14. [Math and random numbers](docs/14-math-and-random.md)

Keep the **[cheat sheet](docs/cheat-sheet.md)** open while you code, and check
**[troubleshooting](docs/troubleshooting.md)** when something goes wrong.
Want your game in its own repository? See
[Using Sprout in your own project](docs/using-sprout-in-your-own-project.md).

### Engine examples

Every example is a complete game with lots of comments. Read them in order:

| Example | What it shows |
|---|---|
| [`01_hello_world`](examples/01_hello_world/main.cpp) | The smallest possible game: a window with some text |
| [`02_shapes_and_colors`](examples/02_shapes_and_colors/main.cpp) | Drawing shapes, text and your own colors |
| [`03_moving_around`](examples/03_moving_around/main.cpp) | Moving with the keyboard, `dt`, `clamp` |
| [`04_pong`](examples/04_pong/main.cpp) | Two-player Pong: your own input actions, bouncing |
| [`05_coin_collector`](examples/05_coin_collector/main.cpp) | Entities, spawning, collisions and a saved high score |
| [`06_space_shooter`](examples/06_space_shooter/main.cpp) | Ships, bullets and asteroids, explosions and screen shake |
| [`07_platformer`](examples/07_platformer/main.cpp) | Tile maps, gravity, jumping and a following camera |
| [`08_snake`](examples/08_snake/main.cpp) | A grid-based game with a growing list |
| [`09_menus_and_settings`](examples/09_menus_and_settings/main.cpp) | Title menu, settings screen and saved settings |
| [`10_sprites_and_sound`](examples/10_sprites_and_sound/main.cpp) | Pictures, animations, sound effects and music |

---

## Folder layout

```
Cpp-Game-engine-/
├── engine/             <- THE ENGINE (code-only)
│   ├── include/sprout/ <- its headers (each starts with a how-to comment)
│   └── src/            <- its code
├── games/              <- your ENGINE games: one folder each, found automatically
│   └── my_first_game/
│       └── main.cpp
├── examples/           <- engine example games 01 to 10, to learn from
├── editor/             <- THE EDITOR (see editor/README.md)
│   ├── src/            <- the editor and its node system
│   ├── player/         <- the game runtime that exported games are built from
│   ├── templates/      <- New Project templates (Multiplayer)
│   └── project/        <- the editor's demo project
├── docs/               <- the documentation (docs/editor/ for the editor)
├── scripts/            <- build, run-editor, test, export, setup-web and test-multiplayer scripts
├── tests/              <- the engine's unit tests
├── cmake/              <- build helpers (downloads libraries, sprout_add_game)
└── CMakeLists.txt      <- the main build file (you normally don't need to touch it)
```

Editor projects don't live in here: they're folders of their own, in
`SproutProjects` in your home folder unless you pick somewhere else.

After building, every game is in `build/bin/<game name>/`, next to a copy of its
`assets` folder.

---

## Built with

Sprout stands on the shoulders of these excellent open-source libraries. CMake
downloads them automatically; you don't need to install them.

| Library | Used for | License |
|---|---|---|
| [raylib](https://github.com/raysan5/raylib) 6.0 by Ramon Santamaria | Window, drawing, input, audio | zlib/libpng |
| [raylib-cpp](https://github.com/RobLoach/raylib-cpp) 6.0.3 by Rob Loach | The C++ wrapper for raylib that Sprout's engine code uses | zlib/libpng |
| [raygui](https://github.com/raysan5/raygui) 5.0 by Ramon Santamaria | Buttons, sliders and other in-game UI | zlib/libpng |
| [nlohmann/json](https://github.com/nlohmann/json) 3.12 by Niels Lohmann | Save files and JSON data | MIT |
| [doctest](https://github.com/doctest/doctest) 2.4 | The engine's unit tests (only when tests are turned on) | MIT |
| [ENet](https://github.com/lsalzman/enet) 1.3 by Lee Salzman | The editor's multiplayer (UDP networking) | MIT |
| [clangd](https://clangd.llvm.org/) 21 (LLVM) | The editor's live script errors and colors (a ready-made download) | Apache 2.0 with LLVM exception |

Web exports from the editor also need [Emscripten](https://emscripten.org),
which the editor's Export window helps you set up.

Sprout's API is inspired by [Godot](https://godotengine.org),
[LÖVE](https://love2d.org), [Pygame Zero](https://pygame-zero.readthedocs.io)
and [raylib](https://www.raylib.com).

## License

Sprout itself is released under the **MIT License** (see [LICENSE](LICENSE)).
That means you can use it for anything, including games you sell, as long as you
keep the license notice. The libraries above have their own (similarly
permissive) licenses.

## Contributing

Found a bug, or want to make Sprout better? Read
[CONTRIBUTING.md](CONTRIBUTING.md) for how the engine is organised, how to run
the tests and the house style.
