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
| New  Open  Save  Save As  Undo Redo      Play  Stop          |
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
collect the coins. Press **Stop** (F8 or Esc) to go back to editing.

## Using it

**Scene dock (top left)** - the node tree.
- **Add** (or Ctrl+A) adds a node as a child of the selected node.
- Duplicate (Ctrl+D), delete (Delete) and move nodes up/down with the buttons.
- Drag a node onto another to make it a child, or onto the top/bottom edge of
  a row to put it before/after that node.
- The little arrow folds a node's children away. A script icon shows which
  nodes have a script (green = compiled, orange = needs a rebuild).

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
- **Attach** picks a compiled script, **New** writes a new script file,
  **Detach** removes the script.

**Toolbar** - New / Open / Save (Ctrl+S) / Save As, Undo (Ctrl+Z) / Redo
(Ctrl+Y or Ctrl+Shift+Z), Play (F5) / Stop (F8, Esc).

## Nodes

| Node | What it's for |
|---|---|
| `Node` | A plain node for grouping or for scripts that don't need a position |
| `Node2D` | Position, rotation (degrees) and scale. Children move with their parent |
| `Sprite2D` | Draws a texture (centered, offset, flip, modulate colour) |
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

Every `.cpp` file in `editor/scripts/` is compiled into the editor. A script is a
class with `ready()` (once, when Play starts) and `process(delta)` (every
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
type a class name. The editor writes `editor/scripts/<name>.cpp` from a
template and attaches it. Then edit the file, rebuild (`cmake --build build`),
restart the editor and press Play.

What a script can use:

| | |
|---|---|
| `node()` | the node it's attached to |
| `self<T>()` | the node as a type, e.g. `self<CharacterBody2D>()` (nullptr if it isn't one) |
| `getNode("Child")`, `getNode("../Sibling")` | nodes by path |
| `findNode("Score")`, `findNode<Label>("Score")` | a node by name anywhere in the scene |
| `node()->queueFree()` | remove the node at the end of the frame |
| `node()->addChild(createNode("Sprite2D"))` | make new nodes while playing |
| `print("text")` | write to the Output panel |
| everything in raylib | input (`IsKeyDown(KEY_SPACE)`), sound, drawing, ... |

Tip: to set a whole vector, write `me->position = Vector2{100, 200};`
(raylib-cpp needs the type name there).

The included scripts are `player_controller.cpp` (platformer movement),
`coin.cpp` (collectibles with a score label) and `spinner.cpp`.

## Files

```
editor/
├── src/
│   ├── main.cpp          <- opens the window and runs the editor
│   ├── editor.*          <- the editor's panels, viewport, undo, dialogs
│   ├── nodes.*           <- the node types
│   ├── scene_tree.*      <- runs a scene: scripts, process, queueFree, drawing
│   ├── script.hpp        <- the Script base class and REGISTER_SCRIPT
│   ├── scene_io.*        <- scenes as JSON files
│   ├── assets.*          <- texture cache
│   └── raygui_*          <- raygui and its dark theme
├── scripts/              <- your C++ scripts (all compiled in)
└── project/
    ├── assets/           <- pictures
    └── scenes/           <- *.scene.json files
```

Scenes are plain JSON (`{"type": "Sprite2D", "name": ..., "properties": {...},
"children": [...]}`), so they're easy to read and to diff.

## Not there yet

This is the bare minimum. Not included: hot-reloading scripts (you rebuild and
restart), script variables in the inspector, instancing scenes inside scenes,
animation, sound nodes, UI (Control) nodes, tilemaps, and exporting a game
without the editor.
