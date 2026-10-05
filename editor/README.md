# Sprout Editor

A small, Godot-style 2D editor built on [raylib](https://www.raylib.com),
[raylib-cpp](https://github.com/RobLoach/raylib-cpp) and
[raygui](https://github.com/raysan5/raygui). It's the bare minimum of an
engine editor: a scene tree of nodes, a live viewport, an inspector, and C++
scripts attached to nodes.

It's separate from the code-only Sprout engine in `engine/`: it has its own
small node system (in `editor/src/`), modelled on Godot's.

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

## Build and run

From the top of the repository (the editor is built with everything else):

```bash
cmake -S . -B build
cmake --build build
./build/bin/sprout_editor/sprout_editor        # Windows: build\bin\sprout_editor\sprout_editor.exe
```

Or build only the editor: `cmake -S editor -B build-editor && cmake --build build-editor`.

It opens `editor/project/` and its scene `scenes/main.scene.json`: a tiny
platformer. Press **Play** (F5), run with the arrow keys, jump with Space and
collect the coins, then walk into the door to reach level 2. Press **Stop** (F8
or Esc) to go back to editing.

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
- **New Project...** makes a new project (with a first scene and the folders
  above) in `SproutProjects` in your home folder, or wherever you choose with
  **Browse...**.
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
  `project/assets/`.
- Click a scene to open it. Click (or drag onto a node) a script to attach it.

**Inspector (right)** - the selected node's fields.
- Drag left/right on a number to change it, or click it to type a value.
- **Attach** picks a loaded script, **New** writes a new script file,
  **Detach** removes the script.

**Toolbar** - New / Open / Save (Ctrl+S) / Save As, Export, Undo (Ctrl+Z) / Redo
(Ctrl+Y or Ctrl+Shift+Z). **Play** (F5) starts the game from its main scene (set
in the Export window), **Play Scene** (F6) plays the scene you're editing, and
**Stop** (F8, Esc) goes back to editing.

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
| Linux | `linux/<Game>` + `project/` | Linux |
| Windows | `windows/<Game>.exe` + `project/` | Windows, or [MinGW-w64](https://www.mingw-w64.org/) on Linux/macOS (`sudo apt install mingw-w64`, `brew install mingw-w64`) |
| macOS | `macos/<Game>.app` | a Mac (Apple's compiler and SDK only run on macOS) |
| Web | `web/index.html` + `.js`, `.wasm`, `.data` | [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) (emsdk) |

The Export window shows which platforms are ready on this computer, and what to
install for the others (then press **Recheck**). It also sets:

- **Game name:** the window title and the program's file name.
- **Main scene:** the scene the game starts with.
- **Window size:** the size of the game's screen (the purple frame in the viewport).
- **Export to:** the folder to export to (default: the project's `exports/`). Each platform
  gets its own folder in it.

These are saved in `project/project.json`. Exporting saves the open scene first.

How it works: the game runtime in `editor/player/` (nodes, scenes and your
scripts, without the editor) is built with CMake for each platform, then
copied next to your `project/` folder (scenes and assets). The first export for a
platform builds raylib for it and takes a minute or two; later exports are quick.
Nothing is downloaded: it reuses the libraries the editor was built with.

- **Windows:** the `.exe` needs nothing installed. Zip the `windows/` folder to
  share it.
- **Web:** browsers won't open the game straight from the disk. Serve the folder
  (`python -m http.server` in it, then open http://localhost:8000) or upload
  it to a site like itch.io as an HTML game.
- **Emscripten** has to be on the PATH, or `EMSDK` set, when the editor starts.
  Start the editor from a terminal where you ran `emsdk_env` (on Windows, web
  exports also need [Ninja](https://ninja-build.org/)).
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
lives), put it in `tree()->values`, which lasts until the game stops:

```cpp
tree()->values["coins"] += 1;
```

The demo has two levels. `door.scene.json` uses the `LevelExit` script; each
door's **next scene** is set in the inspector (under Script Variables). Coins
carry over, and falling off the world restarts the level.

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
| `Area2D` | Detects overlaps: `getOverlappingBodies()`, `getOverlappingAreas()` |

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

To add one: select a node, press **New** under *Script* in the inspector and
type a class name. The editor writes `scripts/<name>.cpp` (in the project) from a
template and attaches it. Then open the file in your code editor.

### Script variables (shown in the inspector)

Like Godot's `@export`, a script can show some of its variables in the
inspector, so each node using it can have its own values. List them in
`exports()`:

```cpp
class PlayerController : public Script {
public:
    float speed = 260;
    std::string nextLevel;

    void exports(Exports& e) override {
        e.add("speed", speed);              // float, int, bool, std::string, Vector2, Color
        e.scene("next_level", nextLevel);   // pick a scene from a list
        // e.texture("icon", iconPath);     // pick a picture from a list
    }
};
```

They appear under **Script Variables** in the inspector. A value you change is
orange, with a button to put the script's default back. Values are saved with the
node in the scene file and put into the variables before `ready()`, also in
exported games and after a hot reload. Instanced scenes can change them per copy,
like any other property. In the demo, each door's `next_scene` picks where it
leads, and the player's `speed`, `jump_speed` and `gravity` can be tuned per
level.

What a script can use:

| | |
|---|---|
| `node()` | the node it's attached to |
| `self<T>()` | the node as a type, e.g. `self<CharacterBody2D>()` (nullptr if it isn't one) |
| `getNode("Child")`, `getNode("../Sibling")` | nodes by path |
| `findNode("Score")`, `findNode<Label>("Score")` | a node by name anywhere in the scene |
| `node()->queueFree()` | remove the node at the end of the frame |
| `node()->addChild(createNode("Sprite2D"))` | make new nodes while playing |
| `node()->addChild(instantiate("scenes/coin.scene.json"))` | make a copy of a scene while playing |
| `changeScene("scenes/level2.scene.json")` | switch to another scene (at the end of this frame) |
| `reloadScene()` | start the current scene again (e.g. when the player dies) |
| `tree()->currentScene()` | the path of the scene that's running |
| `tree()->values["coins"]` | numbers kept across scene changes (score, lives...) |
| `print("text")` | write to the Output panel |
| everything in raylib | input (`IsKeyDown(KEY_SPACE)`), sound, drawing, ... |

Tip: to set a whole vector, write `me->position = Vector2{100, 200};`
(raylib-cpp needs the type name there).

The included scripts are `player_controller.cpp` (platformer movement),
`coin.cpp` (collectibles with a score that carries across levels),
`level_exit.cpp` (a door to the next level) and `spinner.cpp`.

### Hot reloading

Scripts reload while the editor runs. Save a script and, about a second later,
the editor rebuilds them and loads the new code. You don't need to restart it,
and it works while the game is playing too:

- The toolbar shows *Scripts: compiling...*, then *Scripts: N loaded*. The
  Output panel lists the scripts that were reloaded.
- If the code doesn't compile, the compiler's errors appear in the Output panel
  and the last working version keeps running. Fix the error, save, and it
  reloads.
- If the game is playing, every script is made again from the new code and
  its `ready()` runs again. Nodes keep their positions and other properties,
  but the script objects are new, so their member variables start over.
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
│   ├── main.cpp          <- opens the window and runs the editor
│   ├── editor.*          <- the editor's panels, viewport, undo, dialogs
│   ├── nodes.*           <- the node types
│   ├── scene_tree.*      <- runs a scene: scripts, process, queueFree, drawing
│   ├── script.hpp        <- the Script base class and REGISTER_SCRIPT
│   ├── hot_reload.*      <- watches, rebuilds and reloads the scripts library
│   ├── export.*          <- the Export window's work: build + package per platform
│   ├── project.*         <- project.json (game name, main scene, window size)
│   ├── projects.*        <- recent projects, making new ones
│   ├── scene_io.*        <- scenes as JSON files
│   ├── assets.*          <- texture cache
│   ├── theme.*           <- what a theme can set (Theme)
│   └── raygui_*          <- raygui and its widget styles
├── theme/                <- theme files (default_theme.cpp, light_theme.cpp) and their pictures
├── player/               <- the game runtime that exports are built from
├── scripts_module/       <- builds a project's scripts into the hot reload library
└── project/              <- the demo project
    ├── project.json      <- game name, main scene, window size, export settings
    ├── assets/           <- pictures
    ├── scenes/           <- *.scene.json files (main, level2, player, coin, box, door)
    └── scripts/          <- its C++ scripts
```

Scenes are plain JSON (`{"type": "Sprite2D", "name": ..., "properties": {...},
"children": [...]}`), so they're easy to read and to diff.

## Not there yet

This is the bare minimum. Not included: animating other properties (like
Godot's AnimationPlayer), sound nodes, UI (Control) nodes,
tilemaps, and Android/iOS exports.
