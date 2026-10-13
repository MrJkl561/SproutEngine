# Sprout Editor

The Sprout Editor is a small, visual 2D game editor in the style of
[Godot](https://godotengine.org). You build each scene by placing **nodes**
(sprites, rectangles, cameras, physics bodies...) with the mouse, attach **C++
scripts** to them, and press **Play**. Scripts reload while the game runs, and
the **Export** button turns your project into a game for Windows, Linux, macOS
or the web.

> **New here?** Start with the tutorial
> **[Your first game in the editor](../docs/editor/getting-started.md)** (about
> 15 minutes). This page is the manual: come back to it when you want to know
> more about something.

**Editor or Engine?** The editor is separate from the code-only Sprout engine
in [`engine/`](../engine/). It has its own small node system (in
`editor/src/`, namespace `s2d`), modelled on Godot's, and editor projects can't
be opened by the engine or the other way round. See
[Engine or Editor?](../README.md#engine-or-editor) for which to use.

```
+------------------------------------------------------------------------+
| Projects New Open Save Save As Export  Play Play Scene Stop [x2]  gear |
+------------+--------------------------------------------+--------------+
| Scene      |  2D | Script                               | Inspector    |
| (node tree)|                                            | (the selected|
|            |  2D viewport: click, drag, rotate, zoom    |  node's      |
|------------|  or the Script tab: the script editor      |  fields,     |
| FileSystem +--------------------------------------------+  its script, |
| assets,    |  Output | Animation                        |  signals)    |
| scenes,    |                                            |              |
| scripts    |                                            |              |
+------------+--------------------------------------------+--------------+
```

**Contents**

- [Build and run](#build-and-run)
- [Projects](#projects)
- [Using it](#using-it): the docks, the viewport, the toolbar and shortcuts
- [Scenes inside scenes](#scenes-inside-scenes)
- [Exporting your game](#exporting-your-game)
- [Changing scenes while the game runs](#changing-scenes-while-the-game-runs)
- [Autoloads (global scripts)](#autoloads-global-scripts): game state, music and services kept across scenes
- [Animation](#animation)
- [Shaders](#shaders): custom shaders for sprites, rectangles and text
- [Tile maps](#tile-maps): painting levels from tile sheets, and importing maps made in Tiled
- [UI: buttons, menus and HUDs](#ui-buttons-menus-and-huds): the Control nodes and CanvasLayer
- [Multiplayer](#multiplayer): hosting, joining, synced nodes, RPCs, dedicated servers
- [Nodes](#nodes): what each node type is for
- [C++ scripts](#c-scripts): the script editor, script variables (`EXPORT`), signals, hot reloading
- [Customizing the editor](#customizing-the-editor): panel sizes and themes
- [Files](#files): where everything is in `editor/`
- [Not there yet](#not-there-yet)

## Build and run

The editor is built along with everything else. The easiest way is the build
script, which builds it and opens it:

| Windows | macOS / Linux |
|---|---|
| `scripts\run-editor.bat` | `scripts/run-editor.sh` |

Or with CMake, from the top of the repository:

```bash
cmake -S . -B build
cmake --build build
./build/bin/sprout_editor/sprout_editor        # Windows: build\bin\sprout_editor\sprout_editor.exe
```

Or build only the editor: `cmake -S editor -B build-editor && cmake --build build-editor`.
(Need a compiler, CMake and Git first? See
[Installing and building](../docs/01-installing.md).)

The first time, it opens the demo project in `editor/project/`: a tiny
platformer. Press **Play** (F5), run with the arrow keys, jump with Space and
collect the coins, then walk into the door to reach level 2. Press **Stop** (F8
or Esc) to go back to editing. Its scripts (in `editor/project/scripts/`) are
good examples to read.

### Moving the editor, or a portable copy

The editor you build stays connected to the Sprout folder: it compiles your
scripts against the editor's sources and the libraries in there. If you **move
the whole Sprout folder**, it still works (it finds everything again from where
the program is now), no rebuilding needed. Don't copy `sprout_editor.exe` out
of the build folder on its own, though: it needs the rest.

To have an editor you can put anywhere (another drive, a USB stick, another
computer), make a **portable copy**: one folder with the program and
everything it needs.

| Windows | macOS / Linux |
|---|---|
| `scripts\make-portable.bat [folder]` | `scripts/make-portable.sh [folder]` |

It builds the editor and fills `dist/SproutEditor/` (or the folder you give
it) with:

```
SproutEditor/
├── sprout_editor(.exe)   <- start this (Windows: plus sprout_editor.lib, which scripts link to)
├── editor/               <- the editor's sources, templates, demo project, theme
├── cmake/, deps/         <- the libraries (raylib, raylib-cpp, raygui, json, ENet) and clangd
├── README.txt
└── LICENSE
```

About 180 MB. Move or copy that folder wherever you like, as long as its
contents stay together. Your projects aren't stored in it, and each project's
builds go in your user cache folder (`%LOCALAPPDATA%\SproutEditor` on Windows,
`~/Library/Caches/SproutEditor` on macOS, `~/.cache/sprout-editor` on Linux), so
it can even be read-only.

The editor compiles your C++ scripts while it runs, so the computer it runs on
needs the same tools that built it: on Windows, Visual Studio with "Desktop
development with C++" and CMake (Visual Studio's own copy of CMake is found
too); on macOS, Xcode's command line tools and CMake; on Linux, g++ and CMake.
It also has to be the same kind of computer (an editor built on Windows x64
runs on Windows x64). Games you export don't need any of this.

## Projects

Each game is its own project folder:

```
My Game/
├── project.json    <- name, main scene, window size, export settings
├── scenes/         <- *.scene.json
├── assets/         <- pictures
├── scripts/        <- the game's C++ scripts
└── exports/        <- exported games (made by Export)
```

Press **Projects** at the left of the toolbar to open the Project Manager:

- **Recent projects:** click one, then **Open** (or click it a second time).
- **New Project...** makes a new project in `SproutProjects` in your home
  folder, or wherever you choose with **Browse...**. **Start from** picks what's
  in it: **Empty** (a first scene with a camera), **2D Platformer** (a copy of
  the demo game) or **Multiplayer** (players running and jumping together over
  the network). Templates are the folders in `editor/templates/`; add your own
  there, with a `template.json` giving its name and description.
- **Open Folder...** opens any project folder, picked in your system's own
  folder window.
- **Remove from List** only forgets it in the list; the files stay.

The folder windows are the system's own: Explorer's on Windows, Finder's on
macOS, and zenity's or kdialog's on Linux (install one of them, e.g.
`sudo apt install zenity`; without them you type the path instead). The Export
window's **Export to** has a **Browse...** button too.

The editor opens the project you had open last time. You can also name one when
starting it: `sprout_editor "path/to/My Game"`. The demo project lives in
`editor/project/`. Switching projects asks to save unsaved changes, then
reopens the editor on the other project in the same window. Each project's
scripts are built and hot reloaded in a build folder of its own
(`<build>/projects/<name>-<id>/`), so the editor never needs rebuilding for a
new project.

## Using it

**Scene dock (top left)** - the node tree.
- **Add** (or Ctrl+A) adds a node as a child of the selected node.
- Duplicate (Ctrl+D), delete (Delete) and move nodes up/down with the buttons.
- Drag a node onto another to make it a child, or onto the top/bottom edge of
  a row to put it before/after that node.
- The little arrow folds a node's children away. A script icon shows which
  nodes have a script (green = loaded, orange = not loaded yet or failed to compile).

**Viewport (middle)** - the live 2D view. The purple rectangle is the game
window (800 x 450).
- Click a node to select it, drag to move it (hold **Ctrl** to snap to an 8px grid).
- Drag the orange handle above the selected node to rotate it (**Shift** snaps to 15 degrees).
- Arrow keys nudge the selected node (Shift for 10 pixels).
- Mouse wheel zooms, right/middle mouse button (or Space + drag) pans.
- Clicking a sprite inside a physics body selects the body, so they move
  together. Select the child in the Scene dock to move it on its own.

**FileSystem (bottom left)**
- Drag a picture into the viewport to make a `Sprite2D` there. Click a picture
  to give it to the selected `Sprite2D`.
- Drop picture files from your desktop onto the window to import them into
  the project's `assets/` folder.
- Click a scene to open it. Click (or drag onto a node) a script to attach it.
- Right-click a file for more: open it, show it in its folder, or **Delete...**
  (it asks first, then moves the file to the Recycle Bin / Trash, where you can
  get it back from).

**Inspector (right)** - the selected node's fields.
- Drag left/right on a number to change it, or click it to type a value.
- **Attach** picks a loaded script, **New** makes a new script (see
  [New Script](#c-scripts)), **Edit**
  opens it, **Detach** removes the script. Its `EXPORT` variables show under
  **Script Variables**.
- **Signals** lists what the node can announce, with **Connect...** to make a
  script's function get the call ([Signals](#signals)).

**Toolbar** - New / Open / Save (Ctrl+S) / Save As, Export, Undo (Ctrl+Z) / Redo
(Ctrl+Y or Ctrl+Shift+Z). **Play** (F5) starts the game from its main scene (set
in the Export window), **Play Scene** (F6) plays the scene you're editing, and
**Stop** (F8, Esc) goes back to editing. The **window button** next to Stop sets
how many copies of the game Play starts, for multiplayer
([Run Instances](#multiplayer)). Next to it, the scripts' build status, with a
button to rebuild them. The **gear button** has **Code Editor...**, **Run
Instances...**, **Autoloads...** and **Reset Panel Layout**. In a narrow window the buttons show only their
icons. Point at any button to see what it does. Text that's cut short with "..." (long
names, paths) shows in full when you point at it too.

**UI size** - the editor follows your display scaling (for example 150% on
many Windows laptops), and the window opens sized to fit your screen. Change it
with the **- / +** buttons at the top right, or Ctrl + / Ctrl - (Ctrl 0 goes back
to the display's scale). To force a size from the start, set the environment
variable `SPROUT_EDITOR_UI_SCALE`, e.g. `SPROUT_EDITOR_UI_SCALE=1.5`. Press
**Home** in the viewport to fit the game window back in view.

## Scenes inside scenes

Like in Godot, a scene can be placed inside another scene. Build the player
once in `player.scene.json` (body, sprite, collision shape, script), then put
it into every level. The demo works like this: `main.scene.json` uses
`player.scene.json`, `coin.scene.json` (three times), `box.scene.json` and
`door.scene.json`, and so does `level2.scene.json`.

- **Place a scene:** drag a `.scene.json` from the FileSystem into the viewport
  (it goes where you drop it) or onto a node in the Scene dock (it becomes that
  node's child). The scene button next to **Add** does the same from a list.
- **Make a scene out of part of one:** select a node and press **Save Branch
  as Scene** in the inspector. That node and its children are saved to a new
  file and replaced by a copy of it. This is how you split a big level up.
- In the Scene dock a placed scene has a scene icon, and its nodes are tinted.
  Click the arrow to see them. In the viewport, clicking anything in it
  selects the whole copy.

**The original is the template; each copy can be changed on its own:**

- Changing the original scene (open it, edit, save) changes every copy in
  every level.
- Changing a copy in a level only changes that copy. That covers its position
  and also anything inside it: select the copy's sprite in the Scene dock and
  give it another colour, and only that copy changes. The level file stores
  just those differences ("overrides").
- Anything you didn't change in a copy keeps following the original.
- In the inspector, values a copy changed are **orange** with a revert button
  that sets them back to the original's value. **Reset Copy** throws away all of
  the copy's changes (it keeps its position).
- What's *in* a copy (adding, deleting or moving its nodes) comes from the
  original, so change that in the original scene. **Make Local** cuts the link
  and turns the copy into ordinary nodes of the level.
- **Open Original Scene** opens the template. If you have unsaved changes, the
  editor asks whether to save them first.

Scripts can make copies while the game runs:

```cpp
node()->addChild(instantiate("scenes/coin.scene.json"));
```

If a scene file is moved or deleted, the levels using it show "(missing
scene)" in red and keep the link, so putting the file back fixes them. A scene
can't be placed inside itself.

## Exporting your game

Press **Export** in the toolbar to turn the project into a game other people can
play, without the editor:

| Platform | What you get | What this computer needs |
|---|---|---|
| Linux | `linux/<Game>` + `project/` | Linux (it's built for this PC's CPU: x86-64 or ARM64) |
| Windows | `windows/<Game>.exe` + `project/` (x64: runs on every Windows PC, ARM64 ones through emulation) | Windows, or [MinGW-w64](https://www.mingw-w64.org/) on Linux/macOS (`sudo apt install mingw-w64`, `brew install mingw-w64`) |
| Windows ARM64 | `windows-arm64/<Game>.exe` + `project/` (native for Windows on ARM) | Windows with Visual Studio's "MSVC ARM64/ARM64EC build tools" |
| macOS | `macos/<Game>.app` (universal: Apple Silicon and Intel) | a Mac (Apple's compiler and SDK only run on macOS) |
| Web | `web/index.html` + `.js`, `.wasm`, `.data` | [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) (emsdk) |

The Export window shows which platforms are ready on this computer, and what to
install for the others (then press **Recheck**). It also sets:

- **Game name:** the window title and the program's file name.
- **Main scene:** the scene the game starts with.
- **Window size:** the size of the game's screen (the purple frame in the viewport).
- **Export to:** the folder to export to (default: the project's `exports/`). Each platform
  gets its own folder in it.

These are saved in the project's `project.json`. Exporting saves the open scene first.

How it works: the game runtime in `editor/player/` (nodes, scenes and your
scripts, without the editor) is built with CMake for each platform, then
copied next to your `project/` folder (scenes and assets). The first export for a
platform builds raylib for it and takes a minute or two; later exports are quick.
Nothing is downloaded: it reuses the libraries the editor was built with.

- **Windows:** the `.exe` needs nothing installed. Zip the `windows/` folder to
  share it.
- **Dedicated servers:** every Windows, Linux and macOS export can also run as
  a multiplayer server with no window: start it with `--server` (see
  [Multiplayer](#multiplayer)). Web exports can't use multiplayer.
- **Web:** browsers won't open the game straight from the disk. Serve the folder
  (`python -m http.server` in it, then open http://localhost:8000) or upload
  it to a site like itch.io as an HTML game.
- **Emscripten (web):** get emsdk with
  `git clone https://github.com/emscripten-core/emsdk.git` anywhere. The Export
  window finds it in your home folder, Desktop, Documents or `C:\emsdk` (or
  use **Choose...** next to "Emscripten"). If it's freshly cloned, press
  **Set Up**: that runs `emsdk install latest` and `emsdk activate latest` for
  you (about 1 GB to download). You don't need to run `emsdk_env` yourself.
  Emscripten has no Windows ARM64 build, so on Windows the editor always uses
  the x64 one (ARM64 PCs run it through Windows' x64 emulation).
  On Windows, Set Up needs [Python](https://www.python.org/downloads/) for its
  first run, and web builds need Ninja: the one that comes with Visual Studio's
  "C++ CMake tools" is found automatically, or `winget install Ninja-build.Ninja`.
- Exporting from the command line works too, e.g.
  `sprout_editor --export linux,windows,web [folder]`.

## Changing scenes while the game runs

A game is usually several scenes: a title screen, level 1, level 2... A script
switches between them with `changeScene`:

```cpp
changeScene("scenes/level2.scene.json");   // go to another scene
reloadScene();                             // start this scene again
```

The switch happens at the end of the frame, so a script can safely call it on
its own scene. The old scene and all its scripts are freed, then the new scene's
scripts get `ready()`. To keep something from one scene to the next (a score,
lives), use an [autoload](#autoloads-global-scripts), or, for a few numbers,
`tree()->values`, which lasts until the game stops:

```cpp
tree()->values["coins"] += 1;
```

The demo has two levels. `door.scene.json` uses the `LevelExit` script; each
door's **next scene** is set in the inspector (under Script Variables). Coins
carry over, and falling off the world restarts the level.

## Autoloads (global scripts)

Like Godot's autoloads: nodes the game makes when it starts and keeps until it
stops, whatever scene is on. Use one for whatever the whole game shares: the
score and inventory, settings, music that keeps playing between levels,
saving, a global event bus.

**1. Write the script.** Put the class in a header, so other scripts can use
its functions, and register it in a `.cpp` next to it:

```cpp
// scripts/game_state.hpp
#pragma once
#include "script.hpp"
using namespace s2d;

class GameState : public Script {
public:
    EXPORT(int, coins, 0);
    EXPORT(int, lives, 3);
    SIGNAL(coins_changed, int);

    void addCoin() {
        ++coins;
        emitSignal("coins_changed", coins);
    }
};
```

```cpp
// scripts/game_state.cpp
#include "game_state.hpp"
REGISTER_SCRIPT(GameState)
```

**2. Add it:** the **gear button** on the toolbar > **Autoloads...**, pick
`script GameState`, keep the name `GameState`, **Add**. (Or right-click the
script, or a scene, in the FileSystem dock > **Add as Autoload...**.) It's
saved in `project.json`; exported games have it too.

**3. Use it** from any script, in `ready()` or later:

```cpp
#include "game_state.hpp"

class Coin : public Script {
public:
    void ready() override {
        connect(node(), "body_entered", [this](Node* body) {
            if (body->name != "Player") return;
            autoload<GameState>("GameState")->addCoin();
            node()->queueFree();
        });
    }
};
```

and listen to its signals: `connect(autoloadNode("GameState"), "coins_changed", ...)`.

What to know:

- **Made first, kept always.** Autoloads are made in the list's order, and get
  `ready()` before the main scene's scripts, so the scene can use them in its
  own `ready()`. `changeScene` leaves them alone; **Stop** ends them.
- **A scene can be an autoload too** (a HUD, a music player): its nodes are
  drawn on top of the scene. `autoload<Label>("Hud")` gives its root node.
- **Beside the scene, not above it:** the scene's node paths don't change.
  `findNode("GameState")` finds an autoload too, and an autoload's scripts can
  `findNode` the scene's nodes.
- **Hot reloading keeps their `EXPORT` variables** (the scene's scripts start
  over, as before). Other member variables start over, so keep the state that
  matters in `EXPORT`s.
- **A name that isn't there** gives `nullptr`, and the Output panel says so.
- **Not shared between players:** in multiplayer, each copy of the game has its
  own. Send what others need with [RPCs](#multiplayer) or a
  `MultiplayerSynchronizer`.
- For a few numbers, `tree()->values["coins"]` still works without any setup.

## Animation

`AnimatedSprite2D` plays animations from a sprite sheet: one picture with all
the frames in a grid. Selecting one opens the **Animation** panel at the bottom
(next to Output), like Godot's SpriteFrames panel:

```
+--------------------+----------------------------------------+-----------+
| New    Delete      | Stop  Add Frames from Sheet  < > [bin]  |           |
| name [run       ]  |  +-----+ +-----+ +-----+ +-----+        |  preview  |
| fps  [10 ]  [x]loop|  |  1  | |  2  | |  3  | |  4  |        | (playing) |
| idle       (start) |  +-----+ +-----+ +-----+ +-----+        |           |
| run                |   #2      #3      #4      #5            | frame 2/4 |
+--------------------+----------------------------------------+-----------+
```

- **Left:** the animations. **New** / **Delete**, rename, speed (**fps**) and
  **loop**. "(start)" marks the one that plays when the game starts.
- **Middle:** the frames of the picked animation. Click one to show it, move it
  with **<** **>**, remove it with the bin. **Add Frames from Sheet** opens the
  sheet: set its **columns** and **rows**, then click frames in the order they
  should play (numbers show the order), and **Add**. **Play at start** makes the
  picked animation the one the game starts with.
- **Right:** a live preview, with **Play** / **Stop**. The sprite in the viewport
  plays the same animation while you work on it.
- **While the game runs** the panel still works: changes to frames, speed and
  loop show up in the running game straight away (and are kept afterwards).

The same settings are also in the inspector (the sheet with each frame's number,
**animation**, **autoplay**, **speed scale** and the animations list).

From a script:

```cpp
auto* sprite = getNode("Sprite")->as<AnimatedSprite2D>();
sprite->play("run");          // keeps going if "run" is already playing
sprite->stop();
sprite->isPlaying();
sprite->isFinished();         // a non-looping animation reached its last frame
sprite->flipH = true;         // face left
sprite->speedScale = 2;       // twice as fast
```

In the demo the coins spin (`coin_sheet.png`). The player stands, blinks,
runs, jumps and falls (`player_sheet.png`), with `player_controller.cpp`
choosing the animation.

A plain Sprite2D can show one frame of a sheet too (`hframes`, `vframes`,
`frame`), e.g. to pick a tile or change `frame` from a script.

## Shaders

Like Godot's canvas shaders: a small program that decides the colour of each
pixel of a node, for waves, outlines, flashes when hit, dissolves, colour
effects... A `Sprite2D`, `AnimatedSprite2D`, `ColorRect` or `Label` can have one.

**Make one:** select the node, find **Shader** in the Inspector, open the
list and pick **New Shader...**. The editor writes `shaders/<name>.shader`
from a template, gives it to the node and opens it in the Script tab:

```glsl
uniform vec4 tint : source_color = vec4(1.0, 1.0, 1.0, 1.0);
uniform float wave_speed = 3.0;
uniform float wave_size : hint_range(0.0, 0.1) = 0.0;

void fragment() {
    vec2 uv = UV;
    uv.x += sin(uv.y * 20.0 + TIME * wave_speed) * wave_size;
    COLOR = texture(TEXTURE, uv) * MODULATE * tint;
}
```

Change it and save (**Ctrl+S**): the node changes straight away, in the 2D
view and in the running game. A mistake is underlined on its line, and the
Output panel says what's wrong; the node is drawn without the shader until
it's fixed. Another node can use the same file: pick it from the list, or
click the file in the FileSystem dock with the node selected.

**What a shader can use** (it's GLSL, the language of graphics cards):

| | |
|---|---|
| `fragment()` | Runs for every pixel of the node; set `COLOR` |
| `COLOR` | Starts as the picture's colour times the node's modulate; what you set is drawn (`vec4`: red, green, blue, alpha, each 0 to 1) |
| `UV` | Where in the picture this pixel is: `vec2(0.0, 0.0)` top left to `vec2(1.0, 1.0)` bottom right (across the whole rectangle, for a `ColorRect`) |
| `TEXTURE` | The node's picture: `texture(TEXTURE, UV)` reads a colour from it |
| `TEXTURE_PIXEL_SIZE` | One pixel of the picture in UV units (`1.0 / its size`), for outlines and blurs |
| `TIME` | Seconds since the game started, for things that move |
| `MODULATE` | The node's modulate (or a ColorRect's colour, a Label's colour) |

**Parameters:** each `uniform` shows up in the Inspector under **Shader
Parameters**, with its default from the file, and is saved with the node, so
nodes sharing a shader can look different. `float`, `int`, `bool` and `vec2`
show as numbers, `vec3`/`vec4` as a colour, and `sampler2D` as a picture
(another texture to read). `: hint_range(min, max)` sets how fast dragging
changes a number. Scripts can set them too:

```cpp
auto* sprite = self<Sprite2D>();
sprite->setShaderParam("wave_size", 0.05f);
sprite->setShaderParam("tint", Color{255, 80, 80, 255});
sprite->setShaderTexture("noise", "assets/noise.png");
sprite->shader = "shaders/flash.shader";   // or switch shaders
```

Things to know:

- **Desktop and web from one file.** The editor adds what each needs around
  your code (OpenGL 3.3 on Windows, macOS and Linux; WebGL for the web). Write
  numbers with a dot (`1.0`, not `1`): the web is strict about it.
- **One node at a time:** a node's shader draws only that node, not its
  children.
- Not there yet: `vertex()` (moving the corners), reading the screen behind the
  node (`SCREEN_TEXTURE`), and lights.

## Tile maps

A `TileMap` is a grid of tiles cut from pictures, for building levels quickly
(like Godot's TileMap). The step-by-step guide is
**[Tile maps and Tiled](../docs/editor/tilemaps.md)**; the short version:

- **Add a TileMap** (Add > TileMap) and select it: the **TileMap** panel opens
  at the bottom. Press **Add Picture...** and pick a tile sheet (any PNG in the
  project). Set the **tile size**, and the **margin** and **gap** if the sheet
  has empty pixels around or between its tiles.
- **Pick tiles** in the palette on the right (drag across it to pick several
  at once, like a stamp) and **paint** in the viewport.
- **Tools:** Select (click nodes as usual), Paint (left drag paints, right
  drag erases, Shift + drag fills a rectangle), Rectangle, Fill (fills the area
  of matching cells), Erase and Pick (click a cell to paint with its tile).
  The two flip buttons mirror what you paint. Middle drag or Space + drag pans
  while a paint tool is on. Each stroke is one undo step.
- **Collision:** press **Collision** and click tiles in the palette to make
  them solid (they turn red). A `CharacterBody2D`'s `moveAndSlide()` stops at
  solid tiles, just as it does at a `StaticBody2D`. Turn a whole map's
  collision off with its `collision` property.
- **Tile types:** pick one tile and give it a type name ("spikes",
  "ladder") under the tools; scripts ask for it with `getCellType()`.
- A TileMap can use several pictures. Its `cell_size` is the grid's spacing;
  tiles bigger than a cell stick out of the top of it (like in Tiled).
  Rotation is ignored for collisions, like the other bodies.

From a script:

```cpp
auto* map = getNode("TileMap")->as<TileMap>();
Vector2 cell = map->worldToMap(player->globalPosition());  // (column, row)
int below = map->getCell(cell.x, cell.y + 1);              // the tile there, -1 if empty
if (map->getCellType(cell) == "spikes") hurt();            // (cells work as Vector2s too)
map->setCell(3, 4, 7);           // tile 7 of the first picture at column 3, row 4
map->eraseCell(3, 4);
map->isSolid(3, 5);              // a solid tile is there
map->mapToWorld(3, 4);           // the middle of that cell, in the world
map->getUsedCells();             // every cell with a tile
```

### Importing maps from Tiled

[Tiled](https://www.mapeditor.org) is a free map editor. **Drop a `.tmx` or
`.tmj` map onto the editor window** (or keep your Tiled files in the project's
`maps/` folder and drag one from the FileSystem into the viewport, or
right-click it > **Import into This Scene**). The map becomes a `Node2D` with
a node for each layer:

| In Tiled | In Sprout |
|---|---|
| A tile layer | A `TileMap` (with its offset, opacity, tint and visibility) |
| Tiles with a `solid`, `collision` or `collides` property, or collision shapes drawn in Tiled's tile collision editor | Solid tiles |
| A layer named or classed `Collision`, `Solid` or `Walls` (or with a `solid` property) | A TileMap whose tiles are all solid |
| A tile's class (or type) | Its type name, for `getCellType()` |
| Flipped and rotated tiles | Flipped tiles |
| A rectangle (or ellipse) object | An `Area2D` with a `CollisionShape2D` (a `StaticBody2D` if it or its layer is called or classed `solid`, `collision` or `wall`) |
| A tile object | A `Sprite2D` |
| A text object | A `Label` |
| A point, polygon or line | A `Node2D` marking the spot (use its position in scripts) |
| An image layer | A `Sprite2D` |
| A group | A `Node2D` with its layers inside |

Tile sets can be inside the map or in their own `.tsx` / `.tsj` files, and
every tile layer format works except zstd compression (save as CSV or Base64
zlib instead). Infinite maps work too. Pictures outside the project are copied
into `assets/tiles/`. Only orthogonal (square-grid) maps are supported, and
only tile sets made from one picture; tile animations show their first frame.
Importing again makes a fresh copy (delete the old one first). From the
command line: `sprout_editor --project "My Game" --import-tiled level1.tmx`
writes `scenes/level1.scene.json`.

## UI: buttons, menus and HUDs

The UI nodes (Godot's Control nodes) are rectangles: `position` is the top-left
corner and `size` how big. The guide is
**[Menus and HUDs](../docs/editor/ui.md)**.

| Node | What it's for | Signals |
|---|---|---|
| `CanvasLayer` | Its children stay put on the screen while the camera moves: put your HUD and menus inside one. `layer`: higher ones are drawn on top | |
| `Panel` | A box: a color with a border and rounded corners, or a picture stretched with nine-slicing (`texture`, `patch_margin`) | |
| `Button` | Text in a box that can be clicked; `toggle_mode` makes it stay down | `pressed`, `button_down`, `button_up`, `toggled(bool)` |
| `CheckBox` | On or off, with a label | `toggled(bool)` |
| `HSlider` | Drag to pick a number from `min_value` to `max_value` in `step`s | `value_changed(float)` |
| `ProgressBar` | Shows `value` as a bar (health, loading...) | |
| `TextureRect` | A picture: stretched (`stretch_mode` 0), fitted (1), tiled (2) or centred (3) | |
| `LineEdit` | A box to type in (click it first); `placeholder`, `max_length` | `text_changed(std::string)`, `text_submitted(std::string)` |
| `VBoxContainer` / `HBoxContainer` | Puts its children under each other / side by side, `separation` apart, stretching them across (`fill`) and growing to fit | |

Every UI node also has `disabled`, and sends `mouse_entered` and
`mouse_exited`. The `Label` node works inside containers too. Connect the
signals in the Inspector, or in code:

```cpp
void ready() override {
    connect(getNode("Hud/Menu/Start"), "pressed", [this] { changeScene("scenes/level1.scene.json"); });
    connect(getNode("Hud/Menu/Volume"), "value_changed", [](float v) { SetMasterVolume(v / 100); });
}
void process(float) override {
    getNode("Hud/Health")->as<ProgressBar>()->value = health;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !isMouseOverUi()) shoot(getGlobalMousePosition());
}
```

- **The mouse in scripts:** `getMousePosition()` is the mouse in game pixels
  (where CanvasLayer nodes are), `getGlobalMousePosition()` in the world
  (through the camera), and `isMouseOverUi()` is true when it's over a button,
  panel or other UI node, so a click there isn't also a click in the game.
  Use these instead of raylib's `GetMousePosition()`: the game is scaled to
  fit its window (and the editor).
- **Without a CanvasLayer,** UI nodes are part of the world: the camera moves
  them, like a sign with a button on it.
- In the editor, a CanvasLayer's nodes show where they'll be on the screen:
  the game window's frame starts at 0,0.
- Rotation is ignored for UI nodes (scale works).

## Multiplayer

Games can be played together over a network, like in Godot: one copy of the
game hosts and the others join it. The full guide is
**[Multiplayer in the Sprout Editor](../docs/editor/multiplayer.md)**; the short
version:

- **Start from the template:** Projects → New Project... → *Start from*
  **Multiplayer**, then press Play.
- **Host / join** from a script: `multiplayer().host(7777)`,
  `multiplayer().join("192.168.1.20", 7777)`. Scripts hear about it in
  `peerConnected(id)`, `peerDisconnected(id)`, `connectedToServer()`,
  `connectionFailed()` and `serverDisconnected()`.
- **Authority:** each node belongs to one peer (`setMultiplayerAuthority(id)`,
  `isMultiplayerAuthority()`); only that game controls it.
- **`MultiplayerSynchronizer`** keeps the properties you list the same
  everywhere: `position, velocity, Sprite:flip_h, :health`. Other games
  **interpolate** them (0.1 s behind, blending between updates), so other
  players move smoothly.
- **`MultiplayerSpawner`** copies the scenes the server adds below its
  `spawn_path` to every client, including ones that join later.
- **Prediction:** mark what a player controls with `INPUT(float, moveX, 0);`,
  read the keyboard in `gatherInput()` and move in `simulate(delta)`. The
  player's own game moves at once; the server makes the real move from the
  inputs and the player's game replays on top of its answer.
- **Lag compensation:** on the server, in an RPC from a client,
  `lagCompensated([&] { ... })` runs a hit check with everyone moved back to
  where that client saw them.
- **RPCs:** `RPC(jump);` next to `void jump(float height)` lets other games call
  it: `rpc("jump", 300.0f)` (everyone else) or `rpcId(1, "jump", 300.0f)` (the
  server).
- **Run Instances:** the window button next to **Stop** sets how many copies
  Play starts (up to 4), each with its own command line (`--host`,
  `--join 127.0.0.1`, `--server`, `--lag 150` for a slow network). The extra
  copies open in windows of their own and print to the Output panel.
- **Dedicated server:** any exported desktop game started with `--server` runs
  without a window and just hosts. Without exporting:
  `sprout_editor --project "My Game" --play --server`.

It uses [ENet](https://github.com/lsalzman/enet) (UDP), downloaded with the
other libraries. Web exports can't use it (browsers have no UDP).

## Nodes

| Node | What it's for |
|---|---|
| `Node` | A plain node for grouping or for scripts that don't need a position |
| `Node2D` | Position, rotation (degrees) and scale. Children move with their parent |
| `Sprite2D` | Draws a texture (centered, offset, flip, modulate colour), or one frame of a sprite sheet (`hframes`, `vframes`, `frame`) |
| `AnimatedSprite2D` | A Sprite2D that plays named animations from its sprite sheet |
| `ColorRect` | A solid rectangle; good for blocking out levels |
| `Label` | Text |
| `Camera2D` | What part of the world the game shows (zoom, current) |
| `CollisionShape2D` | A rectangle shape for the physics nodes below |
| `StaticBody2D` | Solid, unmoving things: floors, walls, platforms |
| `CharacterBody2D` | Moved by a script with `velocity` and `moveAndSlide()`; slides along static bodies, `isOnFloor()` |
| `Area2D` | Detects overlaps: `getOverlappingBodies()`, `getOverlappingAreas()`, and the signals `body_entered`, `body_exited`, `area_entered`, `area_exited` |
| `Timer` | Counts down `wait_time` seconds and sends `timeout` (again and again, or once with `one_shot`); `autostart`, `start()`, `stop()`, `timeLeft()` |
| `MultiplayerSynchronizer` | Keeps properties (`position`, ...) the same in every player's game ([Multiplayer](#multiplayer)) |
| `MultiplayerSpawner` | Copies the scenes the server adds below a node to every player's game ([Multiplayer](#multiplayer)) |
| `TileMap` | A grid of tiles from tile sheets; paint it in the TileMap panel, solid tiles collide ([Tile maps](#tile-maps)) |
| `CanvasLayer`, `Panel`, `Button`, `CheckBox`, `HSlider`, `ProgressBar`, `TextureRect`, `LineEdit`, `VBoxContainer`, `HBoxContainer` | The UI nodes ([UI](#ui-buttons-menus-and-huds)) |

Physics is simple arcade physics: shapes are axis-aligned rectangles (rotation
is ignored for collisions).

## C++ scripts

Every `.cpp` file in the project's `scripts/` folder is a script. A script is a class with `ready()` (once, when Play starts) and `process(delta)` (every
frame), registered with `REGISTER_SCRIPT`:

```cpp
#include "script.hpp"

using namespace s2d;

class Spinner : public Script {
public:
    float speed = 90;

    void process(float delta) override {
        if (auto* me = self<Node2D>()) me->rotation += speed * delta;
    }
};

REGISTER_SCRIPT(Spinner)
```

To add one: select a node, press **New** under *Script* in the inspector,
type a class name and press **Create**. (Or press **New Script** in the Script
tab, which makes one without attaching it.) The editor writes the file in the
project's `scripts/` folder, named after the class in snake_case
(`PlayerController` is `player_controller.cpp`), from a template, attaches it
and opens it (in the Script tab, or
[your code editor](#using-another-code-editor-vs-code-visual-studio)). Save it
(Ctrl+S) and it's compiled in the background, ready for Play.

The New Script window also asks what the class **inherits** from: `Script`,
or one of your own base classes (see
[Sharing code between scripts](#sharing-code-between-scripts-inheritance)).

**What a script can use** (see `editor/src/script.hpp` for the details):

| | |
|---|---|
| `ready()` | Runs once, when the game starts (after the inspector values are set) |
| `process(delta)` | Runs every frame; `delta` is the seconds since the last one |
| `node()` | The node this script is attached to |
| `self<CharacterBody2D>()` | The same node as a particular type (`nullptr` if it isn't one) |
| `getNode("Sprite2D")`, `getNode("../Door")` | Another node, by path from this one |
| `findNode("Score")`, `findNode<Label>("Score")` | A node by name, anywhere in the scene |
| `node()->queueFree()` | Removes a node at the end of the frame |
| `node()->addChild(createNode("Sprite2D"))` | Makes a new node while playing |
| `node()->addChild(instantiate("scenes/coin.scene.json"))` | Makes a new copy of a scene while playing |
| `changeScene("scenes/level2.scene.json")`, `reloadScene()` | Switch scenes at the end of the frame ([more](#changing-scenes-while-the-game-runs)) |
| `quit()` | Ends the game after this frame (in the editor: stops playing) |
| `getMousePosition()`, `getGlobalMousePosition()`, `isMouseOverUi()` | The mouse on the screen and in the world, and whether it's over a UI node ([more](#ui-buttons-menus-and-huds)) |
| `tree()->currentScene()` | The path of the scene that's running |
| `tree()->values["coins"]` | Numbers that last from one scene to the next |
| `autoload<GameState>("GameState")` | An [autoload](#autoloads-global-scripts): kept for the whole game |
| `print("hello")` | Writes to the Output panel |
| `connect(node, "body_entered", &Coin::onBody)` | Calls a function when a node sends a signal ([more](#signals)) |
| `CALLABLE(onBody);` | Lets signals connected in the editor call that function ([more](#signals)) |
| `SIGNAL(died);`, `emitSignal("died")` | Declares and sends a signal of your own ([more](#signals)) |
| `EXPORT(float, speed, 200);` | A variable shown in the inspector ([more](#script-variables-shown-in-the-inspector)) |
| `multiplayer()`, `RPC(name);`, `rpc(...)`, `INPUT(...)` | Networking ([Multiplayer](#multiplayer)) |
| `hasArg("--host")`, `argValue("--join")` | The game's command line |

**Find nodes in `ready()`, not where a variable is declared.** A member
variable's `= ...` runs when the script is made, before it's on its node, so
`AnimatedSprite2D* sprite = findNode<AnimatedSprite2D>("Hero");` always gets
`nullptr`. Declare it `= nullptr` and set it in `ready()`:

```cpp
AnimatedSprite2D* sprite = nullptr;
void ready() override { sprite = findNode<AnimatedSprite2D>("Hero"); }
```

The Script tab underlines the mistake as you type, and the Output panel says
so if it runs.

Scripts can also call any [raylib](https://www.raylib.com/cheatsheet/cheatsheet.html)
function: `IsKeyDown(KEY_LEFT)`, `IsKeyPressed(KEY_SPACE)`, `GetMousePosition()`,
`PlaySound(...)` and so on.

### Editing scripts (the Script tab)

Sprout has its own script editor, like Godot's: the **2D | Script** switch above
the viewport. Open a file with the **Edit** button next to a node's script in
the Inspector, by right-clicking a script or scene in the FileSystem dock, or by
making a new one with **New Script** at the top right (it opens straight away).
Each open file gets a tab.

- C++ and JSON syntax colors, line numbers, auto-indent, undo/redo
- **Autocomplete** as you type (or **Ctrl+Space**): the engine's classes and
  functions with their comments, raylib, common `std::` names, and what your
  own scripts declare. It follows chains like `self<CharacterBody2D>()->` or
  `body->velocity.` to show the right members. **Enter/Tab** accepts, **Esc**
  closes. It reads the engine headers directly, so new engine functions show
  up by themselves.
- **Ctrl+S** saves (scripts then hot reload by themselves), **Ctrl+Shift+S**
  saves every open file
- **Ctrl+F** find, **Ctrl+H** replace, **F3** next match, **Ctrl+G** go to line,
  **Ctrl+/** comment lines, **Ctrl+D** duplicate line, **Tab/Shift+Tab** indent
- **Brackets and quotes close themselves**: typing `(`, `[`, `{`, `"` or `'`
  adds the closing one after the cursor, and `<` does too after a template
  name (`std::vector<`, `self<`, `template<`) or `#include` (not in `a < b`).
  Typing the closer steps over it, Backspace between an empty pair deletes
  both, and typing one with text selected puts the pair around the selection.
  Not in comments.
- **Errors as you type**: scripts are checked live with
  [clangd](https://clangd.llvm.org/) while you type, before saving. Problems get
  a wavy underline and their message after the line; the whole message, wrapped
  to fit, shows in a strip under the code for the line you're on (or the one
  the mouse is over), and the status bar counts them. clangd also tells the editor what every name is, so members,
  parameters, types, functions, macros and enum values each get their own
  color. Nothing to install: building the editor downloads clangd once (20-90
  MB, into the build folder). Where there's no ready-made clangd (Linux on
  ARM64) one already on the computer is used, or else the compiler checks the
  script (errors only). Turn the download off with `-DSPROUT_EDITOR_CLANGD=OFF`.
  `sprout_editor --project "My Game" --check-file "My Game/scripts/player.cpp"`
  checks one script from the command line.
- **Compile errors** from saving are also listed in the Output panel; clicking
  one jumps to it
- **Scene files** can be edited as JSON. A file that isn't valid JSON, or isn't
  a scene the editor can load, is never saved: the error line is marked
  instead. Saving a scene the 2D view shows (or uses) reloads it there.
- Files changed by another program reload by themselves; if you have unsaved
  changes too, the editor asks which version to keep. Closing a tab, switching
  projects or closing the window asks about unsaved files first.

The colors and font are in the theme file (`t.code...`, see
[Customizing the editor](#customizing-the-editor)).

### Using another code editor (VS Code, Visual Studio...)

For heavier coding (debugging, refactoring, big files) you can open files in
your own editor instead: **gear button** on the toolbar -> **Code Editor...** lists the
editors found on your computer (VS Code, Visual Studio, CLion, Sublime Text, Zed,
Notepad++, Xcode, Kate...), the system default, or a command of your own, where
`{file}` is the file and `{project}` the project folder, e.g.
`code {project} -g {file}`. The choice is saved for you, for every project. The
Edit button and new scripts then open there, and the right-click menu offers
both. Files you change outside Sprout show up in the Script tab by themselves.

**Autocomplete and errors in your own editor work with no setup.** Every time
it opens a project, the Sprout Editor writes the files code editors look for
into the project folder, so they find `script.hpp`, raylib and the C++20
settings:

| File | Used by |
|---|---|
| `compile_commands.json` | Every editor that uses [clangd](https://clangd.llvm.org/): VS Code with the clangd extension, Zed, Sublime Text (LSP), Kate, Neovim, Helix, Qt Creator... and CLion |
| `.vscode/c_cpp_properties.json` | VS Code's Microsoft C/C++ extension |
| `.vscode/settings.json` | VS Code's clangd extension: points it at the clangd that came with the Sprout Editor, so you don't need to install one |
| `CppProperties.json` (Windows) | Visual Studio, when you open the project with **File > Open > Folder** |

How to open a project so your editor picks them up:

- **VS Code:** open the project *folder* (File > Open Folder, or `code "My Game"`).
  The Code Editor setting above does this for you. Use **either** the
  Microsoft C/C++ extension **or** the clangd extension; both work. If you
  have both, clangd offers to turn the other one's IntelliSense off.
- **Visual Studio:** **File > Open > Folder** and pick the project folder (not
  a single file).
- **CLion:** open the project folder; CLion finds `compile_commands.json` and
  loads it as a "compilation database" project.
- **Zed, Sublime Text, Kate, Neovim...:** open the project folder or any script;
  their clangd finds `compile_commands.json` by itself.

These files hold paths on your computer (where the editor was built), so
they're added to the project's `.gitignore` and remade when you open the
project, e.g. after moving the Sprout folder or on another computer. Settings
of your own in the `.vscode` files and `CppProperties.json` are kept. If one
of them has comments, the editor leaves it alone and says so in the Output
panel; delete it and reopen the project to have it made again.

Still seeing "cannot open source file script.hpp"? Open the project in the
Sprout Editor once (that writes the files), then reload your editor's window
(in VS Code: Ctrl+Shift+P, **Developer: Reload Window**).

### Sharing code between scripts (inheritance)

Like `extends` in Godot, a script can inherit from another class, getting
everything it has (its `EXPORT` variables, signals and functions) and changing
or adding to it. Say every enemy sees the player and chases it, but each kind
chases in its own way:

1. **New Script** (Script tab or Inspector), choose **A base class**, name it
   `AI`, **Create**. That makes `scripts/ai.hpp`: shared code, in a header so
   other scripts can include it. It has no `REGISTER_SCRIPT`, so it isn't
   attached to nodes by itself.
2. Write what every enemy does, and leave the parts that differ to a
   `virtual` function:

   ```cpp
   #pragma once
   #include "script.hpp"
   using namespace s2d;

   class AI : public Script {
   public:
       EXPORT(float, speed, 100);
       EXPORT(float, sightRange, 200);
       SIGNAL(spotted, Node*);

       void process(float delta) override {
           auto* me = self<Node2D>();
           auto* player = findNode<Node2D>("Player");
           if (!me || !player) return;
           if (me->position.Distance(player->position) < sightRange) chase(player, delta);
           else wander(delta);
       }

   protected:
       virtual void chase(Node2D* target, float delta) = 0;  // = 0: each enemy must write it
       virtual void wander(float delta) {}                   // optional
   };
   ```

3. **New Script** again, leave **A script**, name it `Slime`, pick **AI**
   under **Inherits**, **Create**. The editor writes `scripts/slime.cpp`
   with `#include "ai.hpp"`, `class Slime : public AI`, and `chase` ready to
   fill in (it lists the functions `AI` lets you change, too):

   ```cpp
   #include "ai.hpp"

   class Slime : public AI {
   public:
       EXPORT(int, damage, 1);  // its own, on top of AI's

   protected:
       void chase(Node2D* target, float delta) override {
           auto* me = self<Node2D>();
           me->position.x += (target->position.x > me->position.x ? 1 : -1) * speed * delta;
       }
   };

   REGISTER_SCRIPT(Slime)
   ```

Attach `Slime` to a node: the Inspector shows `speed` and `sight range` (from
`AI`) and `damage`, and the node has `AI`'s `spotted` signal. A `Bat` can
inherit from `AI` too and chase differently; change `ai.hpp` and every enemy
gets it (they're all rebuilt when you save it).

- Base classes go in `.hpp` files in `scripts/` (they show in the FileSystem
  dock: click one to open it). A base can inherit from another base.
- Only scripts you attach have `REGISTER_SCRIPT`. To attach `AI` itself too,
  add `scripts/ai.cpp` with just `#include "ai.hpp"` and `REGISTER_SCRIPT(AI)`
  (not in the header: every script that includes it would register it again).
- A script that writes its own `ready()` or `process()` replaces the base's;
  call `AI::process(delta);` inside it to keep the base's part too.

### Script variables (shown in the inspector)

Like Godot's `@export`, a script can show some of its variables in the
inspector, so each node using it can have its own values. Declare them with
`EXPORT`: one line makes the variable and shows it.

```cpp
class PlayerController : public Script {
public:
    EXPORT(float, speed, 260);                  // float speed = 260, shown as "speed"
    EXPORT(int, lives, 3);
    EXPORT(bool, canDoubleJump, true);          // shown as "can_double_jump"
    EXPORT(std::string, greeting, "Hello");
    EXPORT(Vector2, launch, 0, -400);           // Vector2{0, -400}
    EXPORT(Color, tint, 255, 200, 200, 255);
    EXPORT_STEP(float, gravity, 1400, 5);       // dragging the number goes 5 at a time
    EXPORT_SCENE(nextLevel, "scenes/level2.scene.json");  // pick a scene from a list
    EXPORT_TEXTURE(skin, "assets/player.png");             // pick a picture from a list

    void process(float delta) override {
        self<Node2D>()->position.x += speed * delta;       // use them like any variable
    }
};
```

`EXPORT` works with `float`, `int`, `bool`, `std::string`, `Vector2` and
`Color` (anything else is an error on that line, while you type). The name in
the inspector and in scene files is the variable's name in snake_case
(`jumpSpeed` is `jump_speed`).

For more control (other names, or variables that only show sometimes), list
them yourself in `exports()` instead, or as well:

```cpp
    float speed = 260;
    void exports(Exports& e) override {
        e.add("speed", speed);              // float, int, bool, std::string, Vector2, Color
        e.scene("next_level", nextLevel);   // e.texture("icon", iconPath) for pictures
    }
```

They appear under **Script Variables** in the inspector. A value you change is
orange, with a button to put the script's default back. Values are saved with the
node in the scene file and put into the variables before `ready()`, also in
exported games and after a hot reload. Instanced scenes can change them per copy,
like any other property. In the demo, each door's `next_scene` picks where it
leads, and the player's `speed`, `jump_speed` and `gravity` can be tuned per
level.

Tip: to set a whole vector, write `me->position = Vector2{100, 200};`
(raylib-cpp needs the type name there).

The demo's scripts are `player_controller.cpp` (platformer movement and
animations), `coin.cpp` (collectibles with a score that carries across levels,
using the Area2D's `body_entered` signal), `level_exit.cpp` (a door to the next
level, also with `body_entered`) and `spinner.cpp`.

### Signals

> **New to signals?** Start with **[Signals, step by step](../docs/editor/signals.md)**:
> what they are, connecting one in the editor and in code, signals of your
> own, and the common mistakes. This section is the reference.

Signals work like Godot's: a node announces that something happened, and
every script that **connected** to it gets a call. The node doesn't need to
know who's listening. `connect(sender, "signal", function)` goes in the
**listening** script, and its first argument is always the node that
**sends** the signal. Connect in `ready()`:

```cpp
class Coin : public Script {
public:
    void ready() override {
        // One of this script's functions...
        connect(node(), "body_entered", &Coin::onBodyEntered);
        // ...or a lambda
        connect(getNode("Timer"), "timeout", [this] { blink(); });
    }

    void onBodyEntered(Node* body) {
        if (body->name == "Player") node()->queueFree();
    }
};
```

**Signals nodes send by themselves** (the Inspector lists a node's signals
under *Signals*):

| Node | Signals |
|---|---|
| every node | `ready` (after its script's `ready()`), `tree_exiting` (it's about to be freed) |
| `Area2D` | `body_entered(Node* body)`, `body_exited(Node* body)`, `area_entered(Node* area)`, `area_exited(Node* area)` |
| `Timer` | `timeout` |
| `AnimatedSprite2D` | `animation_finished`, `animation_looped`, `frame_changed` |
| `MultiplayerSpawner` | `spawned(Node* node)`, `despawned(Node* node)` (on clients) |

**Connecting in the editor** (like Godot's Node dock): select the node that
sends the signal, find the signal under **Signals** in the Inspector and press
**Connect...**. Pick the node whose script should get the call (nodes without
a script are greyed out), check the function's name and press **Connect**:

- The connection is saved in the scene and made when the game starts (before
  `ready()`), and again after a hot reload or scene change.
- If the script doesn't have the function yet, the editor adds it, ready to
  fill in, and shows it in the Script tab:

  ```cpp
  // Called by the "body_entered" signal of Coin2 (connected in the editor)
  CALLABLE(onCoin2BodyEntered);
  void onCoin2BodyEntered(Node* body) {

  }
  ```

  `CALLABLE(name)` is what lets a connection made in the editor call a
  function by its name (C++ can't do that by itself). If the function is
  already there, the editor only adds the `CALLABLE` line above it.
- The Inspector lists each signal's connections under it: click one to see
  its function, **x** to remove it. **Only the first time** makes a one-shot
  connection.
- Renaming or moving nodes in the Scene dock keeps their connections
  pointing at the same nodes. On an instanced scene, connect its root in the
  level; what's inside it is connected in its own scene file.

**Signals of your own.** Declare them in the script with `SIGNAL` (the types
of the values they send, if any), and send them with `emitSignal`. Other
scripts connect to the node the script is on:

```cpp
class Player : public Script {
public:
    SIGNAL(died);
    SIGNAL(health_changed, int);

    void hurt(int damage) {
        health -= damage;
        emitSignal("health_changed", health);
        if (health <= 0) emitSignal("died");
    }
};

// In the HUD's script
connect(findNode("Player"), "health_changed", [this](int hp) { setHearts(hp); });
connect(findNode("Player"), "died", &Hud::showGameOver);
```

Things to know:

- **The function's parameters** are what the signal sends: `Node*` (or any
  node type, like `CharacterBody2D*`: you get `nullptr` if it isn't one),
  `int`, `float`, `bool`, `std::string`, `Vector2`, `Color`. It may take
  **fewer** than the signal sends (`[this] { ... }` for `body_entered` is fine),
  but not more.
- **Once:** `connect(node, "timeout", fn, true)` calls it only the first time.
- **Disconnecting:** `connect` returns an id: `disconnect(id)`. Or
  `disconnect(node, "timeout")` (or just `disconnect(node)`) undoes this
  script's connections to that node.
- **No dangling calls:** a connection goes away by itself when the script
  that made it does (its node is freed, the scene changes, or the script is
  hot reloaded: `ready()` connects again). A freed node sends nothing more.
- **Typos are caught:** connecting to a signal a node doesn't have prints a
  warning with the node's real signals, e.g. `'Tick' (Timer) has no signal
  'timout'. Its signals: ready, tree_exiting, timeout`.
- Signals are called right away, in the order things connected. They're
  for this game only: to tell other players, use an [RPC](#multiplayer).

### Hot reloading

Scripts reload while the editor runs. Save a script and, about a second later,
the editor rebuilds them and loads the new code. You don't need to restart it,
and it works while the game is playing too:

- The toolbar shows *Scripts: compiling...*, then *Scripts: N loaded*. The
  Output panel lists the scripts that were reloaded.
- Pressing **Play** while the scripts are compiling (say, right after saving
  one) waits for them, then plays, so the game always starts with the code you
  just wrote. If they don't compile, it doesn't play and says so; **Stop**
  (F8) cancels the wait.
- If the code doesn't compile, the compiler's errors appear in the Output panel
  and the last working version keeps running. Fix the error, save, and it
  reloads.
- If the game is playing, every script is made again from the new code and
  its `ready()` runs again. Nodes keep their positions and other properties,
  but the script objects are new, so their member variables start over.
  Signal connections are made again too (the ones made in the editor, and the
  ones `ready()` makes).
- Extra windows started by [Run Instances](#multiplayer) keep the scripts they
  started with: Stop and Play again to see the change there.
- The button next to the status rebuilds and reloads by hand.

How it works: the project's scripts are built into a separate library,
`sprout_scripts` (`.dll` on Windows, `.so` on Linux and macOS), by the small
CMake project in `editor/scripts_module/`. The editor watches the project's
`scripts/` folder, runs CMake on it in the background (in the project's own
build folder, with the same build type as the editor), and loads the new
library in place of the old one. Both
the editor and your scripts use the editor's own copy of the nodes and raylib.
The editor makes them available to the library: on Windows by exporting every
raylib function from `raylib.h` and all of the editor's own code.

**Windows notes:**
- You need Visual Studio's compiler (the normal setup). With MinGW, hot
  reloading is off: the scripts of one project (the demo's, or the folder you set
  with `-DSPROUT_EDITOR_STATIC_SCRIPTS_DIR=...`) are compiled into the editor,
  and you rebuild and restart it after changing them.
- If you change the editor's own code (not a script) while it's running, the
  background build can't replace the running `sprout_editor.exe`. Close the
  editor and build normally.
- If the editor runs under the Visual Studio debugger, the debugger can hold the
  scripts' `.pdb` file open, so a rebuild may fail. Run it without debugging
  (Ctrl+F5) for hot reloading.
- Scripts can use everything in `raylib.h`. rlgl and raygui functions aren't
  available to scripts on Windows.

You can pick the mode yourself with `-DSPROUT_EDITOR_HOT_RELOAD=ON/OFF`.
`sprout_editor --check-scripts` loads every script without opening a window
and tells you whether they all work.

## Customizing the editor

### Panel sizes

Drag the borders between the panels (Scene / FileSystem, the side docks, the
Output and Animation panels) to resize them. The editor remembers your layout
for next time (in its settings folder, as `layout.json`). Double-click a
border to put it back to the theme's size.

### Themes: colors, icons, pictures, fonts

The editor's whole look comes from one C++ file,
[`theme/default_theme.cpp`](theme/default_theme.cpp). Edit it, rebuild, and
restart the editor. It sets:

| What | Where in the file |
|------|-------------------|
| Widget look (buttons, text boxes...) | `t.widgets` = one of raygui's 20 styles, plus `t.customizeWidgets` for any `GuiSetStyle()` tweak |
| Font and text size | `t.font` (a .ttf/.otf in `theme/`), `t.fontSize` |
| Panel colors | `t.colors.background`, `panel`, `header`, `border`, `text`, `accent`, `selection`... |
| Script editor | `t.code.background`, `keyword`, `string`, `comment`, `member`, `parameter`..., `t.code.font`, `t.code.fontSize` |
| Viewport colors | `t.viewport.background`, `grid`, axes, game frame, selection outline |
| Gizmos | `t.gizmos.collisionShape`, `camera`... |
| Each node type's color and icon | `t.nodes["Sprite2D"] = {color, ICON_..., "picture.png"}` (a picture replaces the icon) |
| Default panel sizes | `t.layout.leftWidth`, `rightWidth`, `bottomHeight`, `toolbarHeight`, `rowHeight`... |
| Background pictures | `t.images.toolbar`, `panels`, `viewport` (tiled or stretched, with a tint) |
| Window title and icon | `t.title`, `t.images.windowIcon` |

Pictures and fonts go in the theme file's folder and are named by file name.
The icon names are raygui's (`ICON_PLAYER`, `ICON_FILETYPE_IMAGE`... see
`raygui.h`).

To keep your own theme as a separate file, copy `default_theme.cpp` (for
example to `theme/my_theme.cpp`) and choose it when configuring:

```
cmake -S . -B build -DSPROUT_EDITOR_THEME=my_theme.cpp
```

[`theme/light_theme.cpp`](theme/light_theme.cpp) is a second example: a light
theme that only sets what differs from the defaults.

## Files

```
editor/
├── src/
│   ├── main.cpp          <- opens the window and runs the editor; the command-line tools
│   ├── editor.*          <- the editor's panels, viewport, undo, dialogs
│   ├── nodes.*           <- the node types
│   ├── tilemap.*         <- the TileMap node
│   ├── ui_nodes.*        <- the UI nodes (Button, Panel...) and the mouse for them
│   ├── tiled_import.*    <- reads Tiled maps (.tmx, .tmj) and makes nodes from them
│   ├── scene_tree.*      <- runs a scene: scripts, process, queueFree, drawing
│   ├── script.hpp        <- the Script base class, EXPORT, SIGNAL, CALLABLE, RPC, INPUT, REGISTER_SCRIPT
│   ├── signals.*         <- signals: connections, sending, the values they carry
│   ├── shaders.*         <- custom shaders: .shader files to GLSL, their parameters
│   ├── multiplayer.*     <- hosting, joining, RPCs, synchronizers and spawners
│   ├── net_transport.*   <- the network connection under it (ENet)
│   ├── game_runner.*     <- runs a project as a game (exports, Run Instances, --server)
│   ├── console.*         <- Windows: a dedicated server prints to the console it was started from
│   ├── hot_reload.*      <- watches, rebuilds and reloads the scripts library
│   ├── export.*          <- the Export window's work: build + package per platform
│   ├── project.*         <- project.json (game name, main scene, window size)
│   ├── projects.*        <- recent projects, making new ones
│   ├── scene_io.*        <- scenes as JSON files
│   ├── assets.*          <- texture cache
│   ├── theme.*           <- what a theme can set (Theme)
│   ├── code_editor.*     <- opening files in VS Code, Visual Studio...
│   ├── ide_files.*       <- compile_commands.json, .vscode/... so other editors find the headers
│   ├── text_editor.*     <- the Script tab's text editor
│   ├── autocomplete.*    <- its suggestions (reads the engine and raylib headers)
│   ├── script_checker.*  <- live error checking and coloring (clangd)
│   ├── script_lint.*     <- mistakes that compile but don't work (findNode before ready())
│   ├── platform.*        <- operating system calls: libraries, programs, folder windows
│   ├── editor_paths.*    <- finds the editor's own files (also when moved or portable)
│   ├── ui_scale.*        <- the editor's UI size (display scaling)
│   └── raygui_*          <- raygui and its widget styles
├── theme/                <- theme files (default_theme.cpp, light_theme.cpp) and their pictures
├── player/               <- the game runtime that exports are built from
├── templates/            <- New Project templates (multiplayer/)
├── tests/multiplayer/    <- the multiplayer test (scripts/test-multiplayer.sh)
├── tests/signals/        <- the signals test (scripts/test-signals.sh)
├── tests/lint/           <- the script checks' test (scripts/test-lint.sh)
├── tests/autoload/       <- the autoloads test (scripts/test-autoload.sh)
├── tests/shaders/        <- the shaders test (scripts/test-shaders.sh)
├── tests/tilemap/        <- the tile maps and UI test (scripts/test-tilemap.sh), with Tiled maps
├── scripts_module/       <- builds a project's scripts into the hot reload library
└── project/              <- the demo project
    ├── project.json      <- game name, main scene, window size, export settings
    ├── assets/           <- pictures
    ├── scenes/           <- *.scene.json files (main, level2, player, coin, box, door)
    └── scripts/          <- its C++ scripts
```

Scenes are plain JSON (`{"type": "Sprite2D", "name": ..., "properties": {...},
"children": [...]}`, plus `"script"`, `"script_vars"` and `"connections"` when a
node has them), so they're easy to read and to diff.

## Not there yet

This is the bare minimum. Not included: animating other properties (like
Godot's AnimationPlayer), sound nodes, UI anchors and themes (UI nodes are
placed in game pixels, and each sets its own colors), autotiling (terrains),
and Android/iOS exports.
