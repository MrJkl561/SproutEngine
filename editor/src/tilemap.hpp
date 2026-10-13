// TileMap: a grid of tiles cut from pictures (tile sets), like Godot's
// TileMap. Paint it in the editor's TileMap panel (select the node), or import
// a map made in Tiled (drop a .tmx or .tmj file on the editor, or drag one from
// the FileSystem into the viewport).
//
// Tiles marked solid collide: CharacterBody2D::moveAndSlide() stops at them,
// just like at a StaticBody2D. From a script:
//
//     auto* map = getNode("TileMap")->as<TileMap>();
//     Vector2 cell = map->worldToMap(player->globalPosition());
//     if (map->getCell(cell.x, cell.y + 1) == -1) print("Nothing below!");
//     if (map->getCellType(cell) == "spikes") hurt();
//     map->setCell(3, 4, 7);           // tile 7 of the first picture at column 3, row 4
//     map->eraseCell(3, 4);
#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "nodes.hpp"

namespace s2d {

// One picture cut into a grid of tiles (Godot: an atlas source). Tiles are
// numbered from 0, left to right, then top to bottom.
struct TileSource {
    std::string texture;                    // e.g. "assets/tiles.png"
    int tileWidth = 16, tileHeight = 16;    // one tile, in pixels
    int margin = 0;                         // empty pixels around the picture's edge
    int spacing = 0;                        // empty pixels between tiles
    std::set<int> solid;                    // tiles that collide
    std::map<int, std::string> types;       // names for tiles ("spikes", "ladder"), e.g. from Tiled

    int columns() const;                    // tiles across (0 if the picture isn't loaded)
    int rows() const;
    int count() const { return columns() * rows(); }
    Rectangle tileRect(int tile) const;     // where tile is in the picture
};

// What's in one cell: a tile of one of the sources, maybe flipped.
struct TileCell {
    int source = 0;
    int tile = 0;
    // Flips (like Tiled's): 1 = horizontal, 2 = vertical, 4 = diagonal (swaps x and y; done first)
    int flags = 0;
    bool operator==(const TileCell& o) const { return source == o.source && tile == o.tile && flags == o.flags; }
};

class TileMap : public Node2D {
public:
    enum Flip { FlipH = 1, FlipV = 2, FlipDiagonal = 4 };

    raylib::Vector2 cellSize{16, 16};  // the grid's spacing, in pixels
    bool collision = true;             // solid tiles collide with CharacterBody2D
    raylib::Color modulate = WHITE;
    std::vector<TileSource> sources;
    std::map<std::pair<int, int>, TileCell> cells;  // (column, row) -> tile

    const char* typeName() const override { return "TileMap"; }
    void getProperties(std::vector<Property>& out) override;
    void draw(const DrawOptions& options) override;
    Rectangle localRect() const override;  // the used part of the grid

    // --- Cells (columns and rows count from 0 at the node's position) -------
    void setCell(int x, int y, int tile, int source = 0, int flags = 0);
    void eraseCell(int x, int y);
    void clear() { cells.clear(); }
    int getCell(int x, int y) const;          // the tile there, -1 if empty
    int getCellSource(int x, int y) const;    // which picture it's from, -1 if empty
    bool hasCell(int x, int y) const { return cells.count({x, y}) > 0; }
    bool isSolid(int x, int y) const;         // a tile marked solid is there
    std::string getCellType(int x, int y) const;  // the tile's name ("spikes"), "" if none
    // The same, for a cell from worldToMap() / localToMap():
    int getCell(Vector2 cell) const { return getCell(int(cell.x), int(cell.y)); }
    bool isSolid(Vector2 cell) const { return isSolid(int(cell.x), int(cell.y)); }
    std::string getCellType(Vector2 cell) const { return getCellType(int(cell.x), int(cell.y)); }
    void setCell(Vector2 cell, int tile, int source = 0, int flags = 0) { setCell(int(cell.x), int(cell.y), tile, source, flags); }
    void eraseCell(Vector2 cell) { eraseCell(int(cell.x), int(cell.y)); }
    std::vector<raylib::Vector2> getUsedCells() const;
    std::vector<raylib::Vector2> getUsedCellsOfTile(int tile, int source = 0) const;

    // --- Positions ---------------------------------------------------------
    raylib::Vector2 localToMap(Vector2 local) const;   // a point in this node's space -> (column, row)
    raylib::Vector2 mapToLocal(int x, int y) const;    // the middle of a cell, in this node's space
    raylib::Vector2 worldToMap(Vector2 world) const { return localToMap(toLocal(world)); }
    raylib::Vector2 mapToWorld(int x, int y) const { return toGlobal(mapToLocal(x, y)); }

    // --- Tile sets ----------------------------------------------------------
    // Adds a picture cut into tiles; returns its source number.
    int addSource(const std::string& texture, int tileWidth, int tileHeight, int margin = 0, int spacing = 0);
    void setTileSolid(int tile, bool solid, int source = 0);
    void setTileType(int tile, const std::string& type, int source = 0);

    // --- Used by the engine -------------------------------------------------
    // World rectangles of the solid cells that touch area (for moveAndSlide).
    void solidRects(Rectangle area, std::vector<Rectangle>& out) const;
    // (Saved in scene files by tilesToJson / tilesFromJson in scene_io.hpp.)
    // Draws one tile with its top-left corner at local, in this node's space.
    void drawTile(const TileCell& cell, raylib::Vector2 local, Color tint) const;
    // The cells' bounds in cells: {first column, first row, columns, rows}.
    Rectangle usedRect() const;
};

}  // namespace s2d
