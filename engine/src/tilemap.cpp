#include "sprout/tilemap.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <utility>

#include "internal.hpp"
#include "sprout/scene.hpp"

namespace sprout {

TileMap::TileMap(int tileSize, std::vector<std::string> rows) : tileSize_(std::max(1, tileSize)), rows_(std::move(rows)) {
    for (const std::string& row : rows_) columns_ = std::max(columns_, static_cast<int>(row.size()));
    // Make every row the same length so we never read past the end.
    for (std::string& row : rows_) row.resize(static_cast<std::size_t>(columns_), ' ');
}

TileMap TileMap::fromFile(const std::string& path, int tileSize) {
    std::ifstream file(path);
    if (!file) {
        detail::warn("couldn't open the level file \"" + path + "\".");
        return TileMap{tileSize, {}};
    }
    std::vector<std::string> rows;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back(); // Windows line endings
        // Some Windows editors put 3 invisible bytes (a "BOM") at the start of the file.
        if (rows.empty() && line.rfind("\xEF\xBB\xBF", 0) == 0) line.erase(0, 3);
        rows.push_back(line);
    }
    return TileMap{tileSize, std::move(rows)};
}

bool TileMap::inside(int column, int row) const { return column >= 0 && row >= 0 && column < columns_ && row < rows(); }

char TileMap::getTile(int column, int row) const {
    if (!inside(column, row)) return ' ';
    return rows_[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)];
}

void TileMap::setTile(int column, int row, char tile) {
    if (!inside(column, row)) return;
    rows_[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] = tile;
}

TileCoord TileMap::coordAt(Vec2 worldPoint) const {
    const Vec2 local = worldPoint - position;
    const float size = static_cast<float>(tileSize_);
    return TileCoord{static_cast<int>(std::floor(local.x / size)), static_cast<int>(std::floor(local.y / size))};
}

char TileMap::tileAt(Vec2 worldPoint) const {
    const TileCoord c = coordAt(worldPoint);
    return getTile(c.column, c.row);
}

void TileMap::setTileAt(Vec2 worldPoint, char tile) {
    const TileCoord c = coordAt(worldPoint);
    setTile(c.column, c.row, tile);
}

Rect TileMap::tileRect(int column, int row) const {
    const float size = static_cast<float>(tileSize_);
    return Rect{position.x + static_cast<float>(column) * size, position.y + static_cast<float>(row) * size, size, size};
}

Vec2 TileMap::tileCenter(int column, int row) const { return tileRect(column, row).center(); }

std::vector<Vec2> TileMap::findTiles(char tile) const {
    std::vector<Vec2> result;
    for (int row = 0; row < rows(); ++row)
        for (int column = 0; column < columns_; ++column)
            if (getTile(column, row) == tile) result.push_back(tileCenter(column, row));
    return result;
}

void TileMap::replaceTiles(char from, char to) {
    for (std::string& row : rows_) std::replace(row.begin(), row.end(), from, to);
}

int TileMap::countTiles(char tile) const {
    int total = 0;
    for (const std::string& row : rows_) total += static_cast<int>(std::count(row.begin(), row.end(), tile));
    return total;
}

void TileMap::setSolid(const std::string& solidTiles) { solid_ = solidTiles; }

void TileMap::addSolid(char tile) {
    if (!isSolid(tile)) solid_.push_back(tile);
}

bool TileMap::isSolid(char tile) const { return solid_.find(tile) != std::string::npos; }

bool TileMap::isSolidAt(Vec2 worldPoint) const { return isSolid(tileAt(worldPoint)); }

std::vector<Rect> TileMap::solidRectsIn(const Rect& area) const {
    std::vector<Rect> result;
    if (solid_.empty() || rows_.empty()) return result;
    const TileCoord first = coordAt(area.position());
    const TileCoord last = coordAt(Vec2{area.right(), area.bottom()});
    for (int row = std::max(0, first.row); row <= std::min(rows() - 1, last.row); ++row)
        for (int column = std::max(0, first.column); column <= std::min(columns_ - 1, last.column); ++column)
            if (isSolid(getTile(column, row))) result.push_back(tileRect(column, row));
    return result;
}

void TileMap::setColor(char tile, Color color) { looks_[tile].color = color; }

void TileMap::setImage(char tile, const std::string& imagePath) {
    TileLook& look = looks_[tile];
    look.texture = loadTexture(imagePath);
    look.source.reset();
}

void TileMap::setImage(char tile, const std::string& imagePath, const Rect& partOfImage) {
    TileLook& look = looks_[tile];
    look.texture = loadTexture(imagePath);
    look.source = partOfImage;
}

void TileMap::draw() const {
    if (looks_.empty() || rows_.empty()) return;

    // Only draw the tiles the camera can see - big levels stay fast.
    int firstColumn = 0, firstRow = 0, lastColumn = columns_ - 1, lastRow = rows() - 1;
    if (Scene* scene = detail::activeScene()) {
        const Rect view = scene->camera.visibleArea();
        const TileCoord a = coordAt(view.position());
        const TileCoord b = coordAt(Vec2{view.right(), view.bottom()});
        firstColumn = std::max(firstColumn, a.column);
        firstRow = std::max(firstRow, a.row);
        lastColumn = std::min(lastColumn, b.column);
        lastRow = std::min(lastRow, b.row);
    }

    for (int row = firstRow; row <= lastRow; ++row) {
        for (int column = firstColumn; column <= lastColumn; ++column) {
            auto it = looks_.find(getTile(column, row));
            if (it == looks_.end()) continue;
            const TileLook& look = it->second;
            const Rect where = tileRect(column, row);
            if (look.color) drawRect(where, *look.color);
            if (look.texture) {
                const Rect source = look.source.value_or(Rect{0, 0, look.texture->width, look.texture->height});
                drawTexturePart(*look.texture, source, where);
            }
        }
    }
}

} // namespace sprout
