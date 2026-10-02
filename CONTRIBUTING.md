# Contributing to Sprout

Thanks for wanting to make Sprout better! This guide is for people working on
the **engine itself**. If you just want to make games, start with the
[documentation](docs/README.md) instead.

Sprout's one big goal: **a 15-year-old who has never made a game, and knows only
a little C++, can understand it and build a game with it.** Every change is
judged by that. A feature that makes the API harder to read for beginners
needs a very good reason.

---

## How the repository is organised

```
CMakeLists.txt              top-level build: options, then engine/, examples/, games/, tests/
cmake/
  SproutDependencies.cmake  downloads raylib, raylib-cpp, raygui, nlohmann/json (FetchContent)
  SproutGame.cmake          sprout_add_game() and sprout_add_games_in()
engine/
  CMakeLists.txt            the `sprout` static library (alias sprout::sprout)
  include/sprout/           the PUBLIC headers - what game makers read
    sprout.hpp              includes every beginner header
    game.hpp scene.hpp entity.hpp input.hpp graphics.hpp sprite.hpp camera.hpp
    tilemap.hpp physics.hpp particles.hpp audio.hpp timer.hpp ui.hpp save.hpp
    random.hpp math.hpp color.hpp
    raylib_interop.hpp      ADVANCED, opt-in: raylib-cpp + toRaylib()/fromRaylib()
  src/                      the implementation, one .cpp per header
    internal.hpp            private helpers shared by the .cpp files (warn(), windowIsOpen(), ...)
examples/                   example games 01-10, each a folder with main.cpp (+ assets/)
games/                      user games; every sub-folder with a .cpp becomes a game
tests/                      doctest unit tests (built with -DSPROUT_BUILD_TESTS=ON)
docs/                       the beginner documentation
.github/workflows/build.yml CI: builds and tests on Windows, macOS and Linux
```

A few things worth knowing about the engine's insides:

- The engine is built on **raylib-cpp** (the C++ wrapper for raylib). The
  public beginner headers must **not** include raylib or raylib-cpp: only
  `src/` files and the opt-in `raylib_interop.hpp` do. (raylib's global names
  like `Color` and `Texture` would clash with Sprout's.)
- Inside `namespace sprout`, plain `Color` means `sprout::Color`;
  `raylib::Color` is raylib-cpp's and `::Color` is raylib's C struct.
- Global engine state (the window, the current scene, the game clock) lives in
  `src/game.cpp`. `Timer` and `Cooldown` use `detail::gameClock()`, which stops
  while the scene is paused.
- The frame order is in `Game::runWithFactory` (`src/game.cpp`) and
  `Scene::runUpdate` / `Scene::runDraw` (`src/scene.cpp`). The docs describe it
  in [How a game works](docs/03-how-a-game-works.md#everything-that-happens-in-one-frame);
  keep the two in sync.

---

## Building and testing

```bash
cmake -S . -B build -DSPROUT_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The tests use [doctest](https://github.com/doctest/doctest), which CMake
downloads when `SPROUT_BUILD_TESTS` is on. You can also run the test program
directly to pick tests by name: `build/tests/sprout_tests -tc="*Rect*"` (add
`-s` to see passing checks too).

The unit tests cover everything that doesn't need a window or a sound card:
math, colors, random numbers, timers, tile maps, physics, scenes and entities,
sprite animation, particles, the camera, save files, input actions and the
raylib interop conversions. `tests/test_helpers.hpp` has `CaptureStderr` (to
check that a friendly warning was printed) and `TempDir` (a throw-away folder
for file tests).

**Every bug fix should come with a test** that failed before the fix.

### Running games without a screen (headless)

Two environment variables, read in `src/game.cpp`, let you run any game
automatically, for example on a CI server:

| Variable | What it does |
|---|---|
| `SPROUT_TEST_FRAMES=120` | The game quits by itself after 120 frames |
| `SPROUT_TEST_SCREENSHOT=out.png` | Saves a screenshot just before that last frame is shown (needs `SPROUT_TEST_FRAMES`) |

On Linux without a display, use `xvfb-run` (from the `xvfb` package):

```bash
SPROUT_TEST_FRAMES=120 SPROUT_TEST_SCREENSHOT=$PWD/pong.png xvfb-run -a build/bin/04_pong/04_pong
echo $?   # 0 means the game ran and quit normally
```

This is a quick way to check that every example still starts, runs and draws
what you expect after an engine change. Without a sound card the audio quietly
switches off (Sprout prints one warning), so this works in containers too. CI
does exactly this for every game and example on Linux.

### Before you open a pull request

- The engine, tests, examples and `games/my_first_game` build **without
  warnings**. The engine is compiled with `-Wall -Wextra -Wpedantic` (GCC/Clang)
  or `/W4 /permissive-` (MSVC). User games should also stay warning-free with
  `-Wall -Wextra`, so watch out for warnings coming from the headers.
- All tests pass.
- Every example still runs headless (see above).
- If you changed behaviour or the API, the docs in `docs/` (and the
  [cheat sheet](docs/cheat-sheet.md)) say so.

---

## The public headers are documentation

Game makers read `engine/include/sprout/*.hpp` directly, so they must stay
**beginner-readable**:

- **Every header starts with a usage comment** that explains what it's for, in
  plain words, with a short example that a beginner can copy. Keep it up to date
  when you change something.
- Every public function, class and member gets a short `///` comment that says
  what it does in everyday words, including units (pixels, seconds, degrees) and
  what happens in edge cases ("nullptr if there are none", "0 = silent").
- Put the beginner-facing part of a class first. Things only the engine uses go
  under a `// --- Used by the engine ---` heading, implementation details in a
  `detail` namespace, and private members at the bottom.
- Template code that has to live in a header goes at the bottom, under a
  "Template code" banner, so the readable declarations come first.
- Prefer clear names over clever ones: `findClosest`, `isOnFloor`,
  `moveToward`, not `nearest_of_type` or `grounded`.

## API design rules

- **Forgiving by default.** Beginner mistakes (a missing file, an unknown action
  name, loading a picture before the window exists) print a friendly
  `[Sprout] Warning: ...` with `detail::warn()` (once, if it could repeat every
  frame) and carry on with something sensible, such as the pink checkerboard
  texture. Don't crash, and don't throw for things a beginner will plausibly do.
- If the game *must* stop, throw an exception with a message that says how to
  fix it. `Game` catches it and prints "Your game stopped because of an error".
- **Positions are the middle** for entities, sprites, circles and centered
  drawing; **`Rect` is top-left**. Angles are **degrees**, y goes **down**,
  speeds are **per second**.
- Defaults should make the common case a one-liner (see `particles.burst(pos, color)`).
- New features come with: a header comment and `///` comments, tests, a mention
  in the matching docs page and the cheat sheet, and ideally a use in an example.
- Keep the engine small. If something can be done in a few lines of game code
  with the existing API, it probably doesn't need to be in the engine.

## Code style

Match the code around you. In short:

- C++20, 4-space indentation, opening braces on the same line, lines up to about
  120 characters.
- `PascalCase` for types (`TileMap`, `MoveResult`), `camelCase` for functions
  and variables (`moveAndSlide`, `tileSize`), a trailing underscore for private
  members (`tileSize_`), a `k` prefix for constants in `.cpp` files
  (`kMaxParticles`).
- `#include` order: the file's own header, standard library, then other
  libraries and Sprout headers.
- Short comments that explain **why**, written for a curious reader (the
  engine's source is meant to be readable too).
- Use raylib through raylib-cpp in `src/`, and guard raylib calls that need a
  window with `detail::windowIsOpen()`, so functions are safe to call in tests
  and before the `Game` exists.

## Examples and docs

- An example is a folder `examples/NN_name/` with a `main.cpp` (and optionally
  `assets/`). It's built automatically. Start the file with a comment banner
  saying what the example is and "What you'll learn", and comment generously.
- Assets in examples must be your own work or have a license that allows
  redistribution (CC0 is ideal). Note where they came from.
- Docs are GitHub-flavoured Markdown in `docs/`, written in plain, friendly
  English for beginners: short sentences, explain jargon the first time, small
  examples, "Try it!" challenges. Complete programs in the docs must compile
  (with `-Wall -Wextra`, without warnings) and run.

## License

By contributing, you agree that your contributions are released under the
[MIT License](LICENSE), like the rest of Sprout.
