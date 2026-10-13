# Tile maps and Tiled

A **tile map** builds a level out of small square pictures, **tiles**, placed
on a grid: grass, dirt, bricks, water. Instead of placing hundreds of sprites
one by one, you pick a tile and paint with it, like in a drawing program.

The tiles come from a **tile sheet**: one picture with all the tiles side by
side. Sprout cuts it into a grid for you.

This page has three parts:

1. [Painting a level](#1-painting-a-level) in the editor
2. [Making the ground solid](#2-making-the-ground-solid), so the player can stand on it
3. [Importing a map from Tiled](#3-importing-a-map-from-tiled), a free map editor

---

## 1. Painting a level

You need a tile sheet in your project. Any PNG works, as long as its tiles
are all the same size (16x16 pixels is common). Free ones are on
[kenney.nl](https://kenney.nl/assets) and [itch.io](https://itch.io/game-assets/free/tag-tileset).
Drop the PNG onto the editor window: it's copied into `assets/`.

1. Select your scene's root node, press **Add** and pick **TileMap**.
2. The **TileMap** panel opens at the bottom of the editor (if it doesn't,
   click the **TileMap** tab next to Output). Press **Add Picture...** and
   pick your tile sheet.
3. Set **Tile** to the size of one tile in the picture, for example `16` and
   `16`. The palette on the right shows the picture with a grid over it:
   each square should hold exactly one tile. If the sheet has empty pixels
   around its edge, set **Margin**; if it has empty pixels *between* tiles,
   set **Gap**. If the panel says the map's cells are a different size, press
   **Match tiles**.
4. Click a tile in the palette. It gets a yellow outline.
5. Move the mouse over the viewport: a see-through copy of the tile follows
   it, snapped to the grid. **Hold the left button and drag** to paint.
   **Right-drag** erases.

That's it: you're painting a level. A few more things to try:

- **Several tiles at once:** drag across the palette to pick a rectangle of
  tiles (a tree that's 2x3 tiles, say). They're painted together, like a
  stamp.
- **Fill a rectangle:** hold **Shift** while you drag, or pick the
  **Rectangle** tool (the square). Good for big floors.
- **Fill an area:** the **bucket** tool fills every connected cell that holds
  the same tile (or every connected empty cell).
- **Pick a tile from the level:** the **eyedropper** tool. Click a cell, and
  you're painting with its tile.
- **Flip:** the two mirror buttons flip what you paint left-right or upside
  down.
- **Back to selecting:** the **arrow** tool lets you click and drag nodes in
  the viewport again (or just select another node in the Scene dock).
- **Undo:** Ctrl+Z undoes a whole stroke.
- **Panning while painting:** the right button erases, so pan with the
  **middle** button or **Space + drag**.

> **Tip:** use two TileMaps, one for the ground and one called `Decoration`
> on top of it, so flowers and windows don't replace the ground under them.
> Nodes further down the Scene dock are drawn on top.

---

## 2. Making the ground solid

Tiles don't collide until you say which ones are solid.

1. In the TileMap panel, press **Collision**. The palette now shows solid
   tiles in red.
2. Click the tiles that should be solid: ground, walls, platforms. Click
   again to make one not solid. **All** and **None** do the whole picture.
3. Press **Collision** again to go back to painting.

Now a `CharacterBody2D` that calls `moveAndSlide()` stops at those tiles, just
as at a `StaticBody2D`, and `isOnFloor()` is true when it stands on one. The
player script from [Your first game in the editor](getting-started.md) works
unchanged.

### Tiles with a meaning

Some tiles mean something: spikes hurt, ladders can be climbed, a flag ends the
level. Pick one tile in the palette, then type a name in **Tile ... type**
under the tools, for example `spikes`. (Tiles with a type get a small orange
dot in the palette.) A script asks what's under the player:

```cpp
#include "script.hpp"

using namespace s2d;

class Hero : public Script {
public:
    TileMap* map = nullptr;

    void ready() override { map = findNode<TileMap>("Ground"); }

    void process(float delta) override {
        auto* body = self<CharacterBody2D>();
        // ... moving, as in the tutorial ...

        // The cell the player's feet are in
        Vector2 feet = map->worldToMap(body->globalPosition() + Vector2{0, 20});
        if (map->getCellType(feet) == "spikes") reloadScene();
    }
};

REGISTER_SCRIPT(Hero)
```

Scripts can change the map too: `map->setCell(x, y, tile)` puts a tile in a
cell (tile numbers count from 0, left to right, then top to bottom, like the
frames of a sprite sheet) and `map->eraseCell(x, y)` empties it. That's how
you'd make breakable blocks or a door that opens. The full list is in the
[editor manual](../../editor/README.md#tile-maps).

---

## 3. Importing a map from Tiled

[Tiled](https://www.mapeditor.org) is a free, popular map editor with layers,
objects, big tile sets and more. Sprout reads Tiled's maps, so you can build
levels there and play them here.

1. In Tiled, make a map with **Orientation: Orthogonal** and your tile size.
   Add a tile set from your tile sheet, and paint some layers.
2. **To mark solid tiles:** in Tiled's tile set editor, select the tiles and
   add a custom property `solid` (type bool) that's on, or draw a collision
   shape on them in Tiled's tile collision editor. Or put all your walls on
   their own layer called `Collision`: every tile on it becomes solid.
3. Save the map (`.tmx`, or `.tmj` for JSON).
4. **Drag the map file onto the Sprout editor window.** It appears in your
   scene: one node named after the map, with a TileMap for each tile layer.
   Pictures the map uses are copied into the project's `assets/tiles/` folder.

Objects come across too: a rectangle becomes an `Area2D` (good for doors,
checkpoints and level exits; connect its `body_entered` signal), or a
`StaticBody2D` if it's called or classed `solid`, `collision` or `wall`. A
point becomes a plain `Node2D` you can use as a spawn point:

```cpp
auto* spawn = findNode<Node2D>("Spawn");
self<Node2D>()->setGlobalPosition(spawn->globalPosition());
```

> **Keeping Tiled files in the project:** put them in a `maps/` folder in
> the project. They show up in the FileSystem dock; drag one into the
> viewport to import it, or right-click it and pick **Import into This
> Scene**. After changing a map in Tiled, delete the old copy in Sprout and
> import it again.

**What doesn't come across:** maps that aren't orthogonal (isometric,
hexagonal) are imported as square grids, tile sets made of separate pictures
(one per tile) are skipped, and animated tiles show their first frame. Maps
saved with zstd compression can't be read: in Tiled's map properties, set
**Tile Layer Format** to **CSV** or **Base64 (zlib compressed)**. The
[editor manual](../../editor/README.md#importing-maps-from-tiled) has the
full table of what becomes what.
