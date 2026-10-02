# Using Sprout in your own project

The easiest way to make a game is to put it in Sprout's `games/` folder (see
[Your first game](02-your-first-game.md)). But once your game grows, you might
want it in **its own folder or Git repository**, with Sprout as a building block
underneath. This page shows two ways to do that, how to configure Sprout, and
(at the end) how to use raylib-cpp directly for things Sprout doesn't do.

This page assumes you've already built Sprout once by following
[Installing and building](01-installing.md).

---

## Your project's folder

Whichever way you choose, your project looks like this:

```
my_game/
├── CMakeLists.txt     <- tells CMake about your game (below)
├── main.cpp           <- your code
└── assets/            <- pictures, sounds... (copied next to the game when you build)
    └── player.png
```

and you build it exactly like Sprout:

```bash
cmake -S . -B build
cmake --build build
```

Your game ends up in `build/bin/my_game/`, with its `assets` folder next to it.

---

## Option 1: FetchContent (CMake downloads Sprout)

CMake can download Sprout for you, the same way Sprout downloads raylib. Nothing
to copy, and every new copy of your project gets Sprout automatically.

`CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyGame LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a Debug build unless you ask for something else
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Download Sprout (and, through it, raylib, raylib-cpp, raygui and nlohmann/json)
include(FetchContent)
FetchContent_Declare(sprout
    GIT_REPOSITORY https://github.com/MrJkl561/Cpp-Game-engine-.git
    GIT_TAG        main   # a branch name, a version tag or a commit id
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(sprout)

# Your game
sprout_add_game(my_game SOURCES main.cpp)
```

`GIT_TAG` says which version of Sprout to download. Any branch, tag or commit
of the repository works, so if the version you want lives on another branch, put
that branch's name here instead of `main`. A **branch** name (like `main`)
always gets the newest version on that branch. A **commit id** (the long code you see in
the commit history on GitHub) or a release tag never changes, so your game won't
suddenly break when Sprout is updated. That's the safer choice for a finished
game.

`main.cpp` is a normal Sprout game:

```cpp
#include <sprout/sprout.hpp>

using namespace sprout;

struct Hello : Scene {
    void draw() override {
        drawTextureCentered(loadTexture("assets/coin.png"), screenCenter(), 4.0f);
        drawTextCentered("Hello from my own project!", screenCenter() + Vec2{0, 60}, 30);
    }
};

int main() {
    Game game("My Game", 800, 450);
    game.run<Hello>();
}
```

---

## Option 2: `add_subdirectory` (Sprout in a folder of your project)

Put a copy of Sprout inside your project, for example in `external/sprout/`:

```
my_game/
├── CMakeLists.txt
├── main.cpp
├── assets/
└── external/
    └── sprout/        <- a copy of the Sprout folder
```

You can copy the folder, or, if your project uses Git, add Sprout as a
**submodule** (a Git repository inside another one):

```bash
git submodule add https://github.com/MrJkl561/Cpp-Game-engine-.git external/sprout
```

Then `CMakeLists.txt` is the same as above, but with `add_subdirectory` instead
of the FetchContent part:

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyGame LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

add_subdirectory(external/sprout)

sprout_add_game(my_game SOURCES main.cpp)
```

> **Why set `CMAKE_BUILD_TYPE`?** When Sprout is the main project it picks a
> Debug build for you. When it's inside *your* project, that's your job. Without
> it, raylib prints a "Default build type is not set" warning.

---

## `sprout_add_game`

Sprout gives you this CMake command to turn your code into a game:

```cmake
sprout_add_game(<name> [SOURCES file.cpp ...] [ASSETS folder])
```

| Example | What it does |
|---|---|
| `sprout_add_game(my_game)` | Uses **every** `.cpp` file in the current folder |
| `sprout_add_game(my_game SOURCES main.cpp player.cpp)` | Uses just these files |
| `sprout_add_game(my_game ASSETS art)` | Copies the folder `art/` instead of `assets/` (it's still called `assets` next to the game) |

It:

- makes a program called `<name>` that uses the Sprout engine (and C++20),
- puts it in `<build folder>/bin/<name>/`, for Debug and Release builds alike,
- copies your assets folder next to it **on every build**, so changed pictures
  and sounds are picked up even when no code changed,
- makes Visual Studio start the game from that folder.

There's also `sprout_add_games_in(<folder>)`, which turns **every sub-folder**
with `.cpp` files into a game. That's what Sprout's own `games/` and `examples/`
folders use. A sub-folder with its own `CMakeLists.txt` is added with
`add_subdirectory` instead, so you can customise one game when you need to.

You can still use normal CMake commands on your game, for example
`target_compile_definitions(my_game PRIVATE CHEATS=1)`.

> **Without `sprout_add_game`:** you can also link a program to the engine
> yourself with `target_link_libraries(my_game PRIVATE sprout::sprout)`. Then
> copying the assets folder next to the program is up to you.

---

## Options

Set these when you configure, like `cmake -S . -B build -DSPROUT_BUILD_TESTS=ON`,
or with `set(... CACHE BOOL "" FORCE)` before `add_subdirectory`/
`FetchContent_MakeAvailable` in your `CMakeLists.txt`.

| Option | What it does | Default |
|---|---|---|
| `SPROUT_BUILD_GAMES` | Build every game in Sprout's `games/` folder | `ON` when Sprout is the main project, `OFF` inside yours |
| `SPROUT_BUILD_EXAMPLES` | Build Sprout's `examples/` | `ON` when Sprout is the main project, `OFF` inside yours |
| `SPROUT_BUILD_TESTS` | Build the engine's unit tests (downloads doctest) | `OFF` |
| `SPROUT_OUTPUT_DIR` | Where finished games are put | `<build folder>/bin` |
| `SPROUT_RAYLIB_VERSION` | Which raylib to download | `6.0` |
| `SPROUT_RAYLIB_CPP_VERSION` | Which raylib-cpp to download (must match raylib) | `v6.0.3` |
| `SPROUT_RAYGUI_VERSION` | Which raygui to download | `5.0` |
| `SPROUT_JSON_VERSION` | Which nlohmann/json to download | `v3.12.0` |

Changing the library versions is for experts: Sprout is tested with the
defaults.

To run Sprout's own tests (from the Sprout folder):

```bash
cmake -S . -B build -DSPROUT_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## Going further: using raylib-cpp directly

Sprout is built on [raylib](https://www.raylib.com), a popular C library for
games, through [raylib-cpp](https://github.com/RobLoach/raylib-cpp), which
wraps raylib in C++ classes like `raylib::Texture` and `raylib::Rectangle`.
raylib can do a lot more than Sprout shows you: gradients, shaders, render
textures, 3D and more.

**You don't need any of this to make games.** But when you want something
Sprout doesn't have, you can call raylib-cpp (or plain raylib) yourself, right
inside your Sprout game.

### The interop header

Include one extra, **advanced** header. It's not part of `sprout.hpp`, so
beginners never run into it by accident:

```cpp
#include <sprout/sprout.hpp>
#include <sprout/raylib_interop.hpp> // all of raylib-cpp, plus conversions
```

It gives you everything from raylib-cpp (and raylib), plus functions to convert
between Sprout's types and raylib-cpp's:

| Sprout to raylib-cpp | You get |
|---|---|
| `toRaylib(Vec2)` | `raylib::Vector2` |
| `toRaylib(const Rect&)` | `raylib::Rectangle` |
| `toRaylib(Color)` | `raylib::Color` |
| `toRaylib(const Texture&)` | `raylib::TextureUnmanaged`: a view of a Sprout picture. Sprout still owns it, so never `Unload()` it. |
| `toRaylib(const Camera&)` | `raylib::Camera2D`: the scene camera as raylib sees it (including screen shake) |

| raylib to Sprout | You get |
|---|---|
| `fromRaylib(const ::Vector2&)` | `Vec2` |
| `fromRaylib(const ::Rectangle&)` | `Rect` |
| `fromRaylib(const ::Color&)` | `Color` |
| `fromRaylib(const ::Texture&)` | `Texture` |

### Watch out: names that clash

raylib has its own global types with the same names as some of Sprout's:
`Color`, `Texture`, `Camera`, `MouseButton`, `GamepadButton` and
`GamepadAxis`. In a file that includes `raylib_interop.hpp` **and** says
`using namespace sprout;`, the compiler can't tell which one you mean and says
*"reference to 'Color' is ambiguous"*. Write the full name:

```cpp
sprout::Color::Red        // Sprout's color
raylib::Color::Red()      // raylib-cpp's color
isMouseDown(sprout::MouseButton::Left)
```

Everything else (`Vec2`, `Rect`, `Scene`, `Entity`, `drawText`, ...) works as
usual.

### An example: a gradient sky

```cpp
#include <sprout/sprout.hpp>
#include <sprout/raylib_interop.hpp> // advanced: raylib-cpp + conversions

using namespace sprout;

struct GradientScene : Scene {
    float spin = 0;

    void update(float dt) override {
        spin += 90 * dt;
    }

    void draw() override {
        // A sky that fades from light blue (top) to dark blue (bottom)
        sprout::Rect sky{0, 0, 800, 450};
        toRaylib(sky).DrawGradientV(toRaylib(sprout::Color::SkyBlue), toRaylib(sprout::Color::DarkBlue));

        // A spinning ring, using a plain raylib function
        DrawRing(toRaylib(Vec2{400, 225}), 40, 60, spin, spin + 270, 32, toRaylib(sprout::Color::Gold));

        // Sprout drawing still works as normal
        drawCircle({400, 225}, 20, sprout::Color::White);
    }

    void drawUI() override {
        drawText("Gradients and rings come straight from raylib", 10, 10, 20);
    }
};

int main() {
    Game game("raylib-cpp inside Sprout", 800, 450);
    game.run<GradientScene>();
}
```

### Rules for mixing

- **Draw only in `draw()` and `drawUI()`.** In `draw()`, raylib drawing goes
  through the scene camera, just like Sprout's; in `drawUI()` it doesn't.
- **Let Sprout own the window, the frame and the sound device.** Don't call
  `InitWindow`, `CloseWindow`, `BeginDrawing`, `EndDrawing`, `InitAudioDevice` or
  `CloseAudioDevice`, and don't make a `raylib::Window` or `raylib::AudioDevice`.
- **Load raylib things after the `Game` is created** (for example in `start()`),
  and keep them as members of your scene or entity, so they're freed before the
  window closes. raylib-cpp classes like `raylib::Texture` and `raylib::Shader`
  free themselves when they're destroyed.
- Pictures loaded with Sprout's `loadTexture` belong to Sprout. Use
  `toRaylib(texture)` to draw them with raylib, but don't unload them.

Useful links: the [raylib cheatsheet](https://www.raylib.com/cheatsheet/cheatsheet.html)
lists every raylib function, the [raylib examples](https://www.raylib.com/examples.html)
show them in action, and the [raylib-cpp documentation](https://robloach.github.io/raylib-cpp/)
lists its classes.
