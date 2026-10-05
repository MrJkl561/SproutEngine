# Sprout

**A friendly C++ game engine for people making their very first games.**

Sprout is a small, code-only 2D game engine for C++20. There's no editor to
learn: you write a short C++ file, build it, and play. It's designed so that
someone who has never made a game, and only knows a little C++, can make one in
an afternoon. Its ideas come from engines that beginners love: **scenes,
entities, input actions and `moveAndSlide`** from [Godot](https://godotengine.org),
and the "just write a few functions" style of [LÖVE](https://love2d.org),
[Pygame Zero](https://pygame-zero.readthedocs.io) and [raylib](https://www.raylib.com),
which Sprout is built on.

Here's a complete game. Move the circle with the arrow keys, and press Space for
fireworks:

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

---

## Features

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
- Runs on **Windows, macOS and Linux**. Every library is downloaded
  automatically by CMake.

**What Sprout isn't:** it's for 2D desktop games written in code. There's no
3D, no networking and no web or mobile export. (If you
outgrow it, you can call [raylib-cpp](https://github.com/RobLoach/raylib-cpp)
directly from a Sprout game, see
[Using raylib-cpp](docs/using-sprout-in-your-own-project.md#going-further-using-raylib-cpp-directly).)

---

## Quick start

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

**3. Play:**

| | Windows | macOS / Linux |
|---|---|---|
| Your first game | `build\bin\my_first_game\my_first_game.exe` | `./build/bin/my_first_game/my_first_game` |
| An example | `build\bin\04_pong\04_pong.exe` | `./build/bin/04_pong/04_pong` |

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

## The editor (experimental)

There's also a small **Godot-style 2D editor** in [`editor/`](editor/README.md):
a scene tree of nodes (`Node2D`, `Sprite2D`, `Camera2D`, `CharacterBody2D`, ...),
a live viewport where you drag textures around, an inspector, and C++ scripts
attached to nodes that reload while it runs. It's built along with everything else:

| Windows | macOS / Linux |
|---|---|
| `build\bin\sprout_editor\sprout_editor.exe` | `./build/bin/sprout_editor/sprout_editor` |

## Make your own game

1. Copy the folder `games/my_first_game` and give the copy a new name, like
   `games/space_cats`.
2. Build again (`cmake -S . -B build` then `cmake --build build`). Sprout finds
   the new folder automatically.
3. Run `build/bin/space_cats/space_cats`, and start changing `main.cpp`!

Put pictures and sounds in an `assets` folder next to your `main.cpp`
(`games/space_cats/assets/`). It's copied next to your game every time you
build, so `loadTexture("assets/cat.png")` just works.

New to all this? Follow the tutorial **[Your first game](docs/02-your-first-game.md)**:
you'll build "Catch the Falling Stars" from an empty file, one step at a time.

---

## Learn

The **[documentation](docs/README.md)** is written for beginners. Start at the
top and work your way down:

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

### Examples

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
├── games/              <- YOUR games: one folder each, found automatically
│   └── my_first_game/
│       └── main.cpp
├── examples/           <- example games 01 to 10, to learn from
├── docs/               <- the documentation
├── editor/             <- the Godot-style 2D editor (see editor/README.md)
├── engine/
│   ├── include/sprout/ <- the engine's headers (each starts with a how-to comment)
│   └── src/            <- the engine's code
├── tests/              <- the engine's unit tests
├── cmake/              <- build helpers (downloads libraries, sprout_add_game)
└── CMakeLists.txt      <- the main build file (you normally don't need to touch it)
```

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
