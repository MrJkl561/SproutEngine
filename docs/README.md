# Sprout documentation

Welcome! These pages teach you how to make games with Sprout, even if you've
never made a game before. You only need to know a little C++: what a variable
is, what an `if` does, and roughly what a function is. Every other idea is
explained the first time it shows up.

## Engine or Editor?

Sprout has two separate parts, and you make games differently with each:

- The **Sprout Engine** (`engine/`) is code-only: a game is a C++ file in
  `games/` with a `Scene` that has `update()` and `draw()`. Most of these pages
  are about the engine.
- The **Sprout Editor** (`editor/`) is visual, like a small Godot: you place
  nodes in a scene with the mouse, attach C++ scripts to them and press Play.
  It has its own node system and its own docs, below.

A game made with one can't be opened by the other. Not sure which to pick? See
[Engine or Editor?](../README.md#engine-or-editor) in the main README.

## Learning the Editor

| | Page | What you'll learn |
|---|---|---|
| 1 | [Installing and building](01-installing.md) | Installing the tools and building Sprout (the same for both parts) |
| 2 | [Your first game in the editor](editor/getting-started.md) | A step-by-step tutorial: projects, nodes, a script, Play, Export |
| 3 | [Signals, step by step](editor/signals.md) | What a signal is, connecting one in the editor and in code, signals of your own, common mistakes |
| 4 | [Tile maps and Tiled](editor/tilemaps.md) | Painting levels from a tile sheet, solid tiles, tiles that mean something, importing maps made in Tiled |
| 5 | [Menus and HUDs](editor/ui.md) | Buttons, panels, sliders and health bars; a HUD that stays on screen; a title menu; making it look like your game |
| 6 | [Multiplayer in the editor](editor/multiplayer.md) | Hosting and joining, synced nodes, spawners, RPCs, prediction, lag compensation, testing with several windows, dedicated servers |
| 7 | [The Sprout Editor manual](../editor/README.md) | Everything else: every dock and shortcut, nodes, the script API, signals, scenes inside scenes, animation, exporting, themes |

## Learning the Engine

Work through the pages in order. Each one builds on the ones before it.

### Getting started

| | Page | What you'll learn |
|---|---|---|
| 1 | [Installing and building](01-installing.md) | Installing the tools, getting Sprout, building and running a game |
| 2 | [Your first game](02-your-first-game.md) | A step-by-step tutorial: build "Catch the Falling Stars" from an empty file |
| 3 | [How a game works](03-how-a-game-works.md) | The game loop, frames, delta time (`dt`) and coordinates |

### The building blocks

| | Page | What you'll learn |
|---|---|---|
| 4 | [Scenes](04-scenes.md) | Title screens, levels and game-over screens, switching between them, pausing |
| 5 | [Entities](05-entities.md) | Players, enemies and coins: spawning, finding, destroying, collisions |
| 6 | [Input](06-input.md) | Keyboard, mouse, gamepads and input actions |
| 7 | [Drawing](07-drawing.md) | Shapes, colors, text, fonts, pictures, sprites and animation |
| 8 | [Sound and music](08-sound-and-music.md) | Sound effects and background music |

### Making it a real game

| | Page | What you'll learn |
|---|---|---|
| 9 | [Levels and physics](09-levels-and-physics.md) | Levels made of letters, walls, gravity and jumping |
| 10 | [Camera and effects](10-camera-and-effects.md) | Following the player, zoom, screen shake and particles |
| 11 | [Timers](11-timers.md) | Doing things later, again and again, or not too often (and what lambdas are) |
| 12 | [Menus and UI](12-menus-and-ui.md) | Buttons, sliders, checkboxes, text boxes and themes |
| 13 | [Saving](13-saving.md) | High scores, settings, and JSON files for your own data |
| 14 | [Math and random numbers](14-math-and-random.md) | `Vec2`, `Rect`, angles, handy helpers and randomness |

### Reference and help

| Page | When to use it |
|---|---|
| [Cheat sheet](cheat-sheet.md) | Every Sprout function on one page. Keep it open while you code! |
| [Troubleshooting](troubleshooting.md) | Something doesn't build, crashes or behaves strangely |
| [Using Sprout in your own project](using-sprout-in-your-own-project.md) | Your game in its own repository, CMake options, and using raylib-cpp directly |
| [Contributing](../CONTRIBUTING.md) | You want to improve the engine itself |

## Learn from the engine examples

The `examples/` folder has ten complete, well-commented games. They match the
pages above nicely:

| After reading... | Look at... |
|---|---|
| Your first game, How a game works | `01_hello_world`, `02_shapes_and_colors`, `03_moving_around` |
| Scenes, Entities, Input | `04_pong`, `05_coin_collector` |
| Camera and effects, Timers | `06_space_shooter` |
| Levels and physics | `07_platformer` |
| Math and random numbers | `08_snake` |
| Menus and UI, Saving | `09_menus_and_settings` |
| Drawing, Sound and music | `10_sprites_and_sound` |

They're all built along with Sprout, so you can run them straight away from
`build/bin/`.

## Tips for learning

- **Type the code yourself** instead of copying it. It's slower, but you'll
  remember much more, and you'll learn to spot typos.
- **Change things and see what happens.** Make the numbers bigger, swap the
  colors, delete a line. You can't break anything that undo can't fix.
- **Do the "Try it!" challenges.** They're where the real learning happens.
- **Make small games first.** Pong, Snake and Breakout teach you more than a
  huge unfinished dream game. The dream game will be much easier afterwards!
- **Errors are normal.** Every programmer sees error messages all day long. Read
  the first one carefully, and check [Troubleshooting](troubleshooting.md).
- **The headers are documentation too.** Each file in `engine/include/sprout/`
  starts with a comment explaining how to use it, with examples.

Have fun, and happy game making!
