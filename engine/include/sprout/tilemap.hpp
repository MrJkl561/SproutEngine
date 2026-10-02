#pragma once
// =============================================================================
//  sprout/tilemap.hpp - build levels out of letters
//
//  Draw your level with characters. Each character is one tile:
//
//    TileMap level{32, {                 // every tile is 32x32 pixels
//        "####################",
//        "#..................#",
//        "#..P.........C.....#",
//        "#......#####.......#",
//        "####################",
//    }};
//
//    level.setSolid("#");                    // players can't walk through '#'
//    level.setColor('#', Color::DarkGray);   // draw '#' as gray squares
//    level.setImage('C', "assets/coin.png"); // ...or as a picture
//
//    for (Vec2 spot : level.findTiles('P')) spawn<Player>(spot);
//    level.replaceTiles('P', '.');           // remove the marker
//
//  Then draw it in your scene's draw():     level.draw();
//  and let entities collide with it:        addSolid(level);   (in start())
//
//  '.' and ' ' are empty tiles and are never drawn (unless you give them a color).
// =============================================================================
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "sprout/color.hpp"
#include "sprout/graphics.hpp"
#include "sprout/math.hpp"

namespace sprout {

/// A tile's place in the grid. column 0 is the left edge, row 0 is the top.
struct TileCoord {
    int column = 0;
    int row = 0;
    bool operator==(const TileCoord&) const = default;
};

class TileMap {
public:
    /// The world position of the map's top-left corner.
    Vec2 position;

    TileMap() = default;
    /// Makes a map from rows of text. tileSize is in pixels.
    TileMap(int tileSize, std::vector<std::string> rows);

    /// Loads a map from a text file (one row of tiles per line).
    static TileMap fromFile(const std::string& path, int tileSize);

    // --- Size -----------------------------------------------------------------
    int tileSize() const { return tileSize_; }
    int columns() const { return columns_; }
    int rows() const { return static_cast<int>(rows_.size()); }
    float width() const { return static_cast<float>(columns_ * tileSize_); }
    float height() const { return static_cast<float>(rows() * tileSize_); }
    /// The whole map as a rectangle in the world.
    Rect bounds() const { return Rect{position, Vec2{width(), height()}}; }

    // --- Reading and changing tiles ---------------------------------------------
    /// The tile at a column and row. Outside the map you get ' '.
    char getTile(int column, int row) const;
    void setTile(int column, int row, char tile);
    /// The tile under a point in the world.
    char tileAt(Vec2 worldPoint) const;
    void setTileAt(Vec2 worldPoint, char tile);
    /// Which column/row a world point is in.
    TileCoord coordAt(Vec2 worldPoint) const;
    /// The middle of a tile, in world coordinates.
    Vec2 tileCenter(int column, int row) const;
    /// The rectangle a tile covers, in world coordinates.
    Rect tileRect(int column, int row) const;
    /// The middle of every tile with this character. Great for placing things.
    std::vector<Vec2> findTiles(char tile) const;
    /// Changes every `from` tile into a `to` tile.
    void replaceTiles(char from, char to);
    /// How many tiles of this kind there are.
    int countTiles(char tile) const;

    // --- Collision --------------------------------------------------------------
    /// Which characters are solid walls/floors, e.g. setSolid("#=B").
    void setSolid(const std::string& solidTiles);
    /// Adds one more solid character.
    void addSolid(char tile);
    bool isSolid(char tile) const;
    bool isSolidAt(Vec2 worldPoint) const;
    /// Rectangles of all solid tiles touching `area`. (Used by moveAndCollide.)
    std::vector<Rect> solidRectsIn(const Rect& area) const;

    // --- Looks ------------------------------------------------------------------
    /// Draws this kind of tile as a colored square.
    void setColor(char tile, Color color);
    /// Draws this kind of tile using a picture (stretched to the tile size).
    void setImage(char tile, const std::string& imagePath);
    /// Draws this kind of tile using part of a picture (for tile sets).
    void setImage(char tile, const std::string& imagePath, const Rect& partOfImage);
    /// Draws every tile that has a color or picture. Call it in draw().
    void draw() const;

private:
    struct TileLook {
        std::optional<Color> color;
        std::optional<Texture> texture;
        std::optional<Rect> source;
    };

    bool inside(int column, int row) const;

    int tileSize_ = 32;
    int columns_ = 0;
    std::vector<std::string> rows_;
    std::string solid_;
    std::map<char, TileLook> looks_;
};

} // namespace sprout
