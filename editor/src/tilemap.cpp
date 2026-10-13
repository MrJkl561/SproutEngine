#include "tilemap.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include <rlgl.h>

#include "assets.hpp"
#include "scene_io.hpp"
#include "scene_tree.hpp"

namespace s2d {

using nlohmann::json;

// ---------------------------------------------------------------------------
// TileSource
// ---------------------------------------------------------------------------
int TileSource::columns() const {
    raylib::Texture* tex = Assets::texture(texture);
    if (!tex || tileWidth <= 0) return 0;
    return std::max(0, (tex->width - 2 * margin + spacing) / (tileWidth + spacing));
}

int TileSource::rows() const {
    raylib::Texture* tex = Assets::texture(texture);
    if (!tex || tileHeight <= 0) return 0;
    return std::max(0, (tex->height - 2 * margin + spacing) / (tileHeight + spacing));
}

Rectangle TileSource::tileRect(int tile) const {
    int cols = std::max(columns(), 1);
    return {static_cast<float>(margin + (tile % cols) * (tileWidth + spacing)),
            static_cast<float>(margin + (tile / cols) * (tileHeight + spacing)), static_cast<float>(tileWidth),
            static_cast<float>(tileHeight)};
}

// ---------------------------------------------------------------------------
// TileMap
// ---------------------------------------------------------------------------
void TileMap::getProperties(std::vector<Property>& out) {
    Node2D::getProperties(out);
    out.push_back({"TileMap", PropKind::Section});
    out.push_back({"cell_size", PropKind::Vec2, &cellSize, 1.0f});
    out.push_back({"collision", PropKind::Bool, &collision});
    out.push_back({"modulate", PropKind::Color, &modulate});
    out.push_back({"tiles", PropKind::Tiles, this});
    addShaderProperties(out);
}

void TileMap::setCell(int x, int y, int tile, int source, int flags) {
    if (tile < 0) {
        eraseCell(x, y);
        return;
    }
    cells[{x, y}] = TileCell{source, tile, flags};
}

void TileMap::eraseCell(int x, int y) { cells.erase({x, y}); }

int TileMap::getCell(int x, int y) const {
    auto it = cells.find({x, y});
    return it == cells.end() ? -1 : it->second.tile;
}

int TileMap::getCellSource(int x, int y) const {
    auto it = cells.find({x, y});
    return it == cells.end() ? -1 : it->second.source;
}

bool TileMap::isSolid(int x, int y) const {
    auto it = cells.find({x, y});
    if (it == cells.end()) return false;
    const TileCell& c = it->second;
    return c.source >= 0 && c.source < static_cast<int>(sources.size()) && sources[c.source].solid.count(c.tile) > 0;
}

std::string TileMap::getCellType(int x, int y) const {
    auto it = cells.find({x, y});
    if (it == cells.end()) return "";
    const TileCell& c = it->second;
    if (c.source < 0 || c.source >= static_cast<int>(sources.size())) return "";
    auto t = sources[c.source].types.find(c.tile);
    return t == sources[c.source].types.end() ? "" : t->second;
}

std::vector<raylib::Vector2> TileMap::getUsedCells() const {
    std::vector<raylib::Vector2> out;
    out.reserve(cells.size());
    for (auto& [pos, c] : cells) out.emplace_back(static_cast<float>(pos.first), static_cast<float>(pos.second));
    return out;
}

std::vector<raylib::Vector2> TileMap::getUsedCellsOfTile(int tile, int source) const {
    std::vector<raylib::Vector2> out;
    for (auto& [pos, c] : cells)
        if (c.tile == tile && c.source == source)
            out.emplace_back(static_cast<float>(pos.first), static_cast<float>(pos.second));
    return out;
}

raylib::Vector2 TileMap::localToMap(Vector2 local) const {
    float cw = cellSize.x > 0 ? cellSize.x : 1, ch = cellSize.y > 0 ? cellSize.y : 1;
    return {std::floor(local.x / cw), std::floor(local.y / ch)};
}

raylib::Vector2 TileMap::mapToLocal(int x, int y) const {
    return {(x + 0.5f) * cellSize.x, (y + 0.5f) * cellSize.y};
}

int TileMap::addSource(const std::string& texture, int tileWidth, int tileHeight, int margin, int spacing) {
    TileSource s;
    s.texture = texture;
    s.tileWidth = std::max(1, tileWidth);
    s.tileHeight = std::max(1, tileHeight);
    s.margin = std::max(0, margin);
    s.spacing = std::max(0, spacing);
    sources.push_back(s);
    return static_cast<int>(sources.size()) - 1;
}

void TileMap::setTileSolid(int tile, bool solid, int source) {
    if (source < 0 || source >= static_cast<int>(sources.size())) return;
    if (solid) sources[source].solid.insert(tile);
    else sources[source].solid.erase(tile);
}

void TileMap::setTileType(int tile, const std::string& type, int source) {
    if (source < 0 || source >= static_cast<int>(sources.size())) return;
    if (type.empty()) sources[source].types.erase(tile);
    else sources[source].types[tile] = type;
}

Rectangle TileMap::usedRect() const {
    if (cells.empty()) return {0, 0, 0, 0};
    int x0 = cells.begin()->first.first, x1 = x0, y0 = cells.begin()->first.second, y1 = y0;
    for (auto& [pos, c] : cells) {
        x0 = std::min(x0, pos.first);
        x1 = std::max(x1, pos.first);
        y0 = std::min(y0, pos.second);
        y1 = std::max(y1, pos.second);
    }
    return {static_cast<float>(x0), static_cast<float>(y0), static_cast<float>(x1 - x0 + 1),
            static_cast<float>(y1 - y0 + 1)};
}

Rectangle TileMap::localRect() const {
    Rectangle u = usedRect();
    return {u.x * cellSize.x, u.y * cellSize.y, u.width * cellSize.x, u.height * cellSize.y};
}

void TileMap::solidRects(Rectangle area, std::vector<Rectangle>& out) const {
    if (!collision || cells.empty() || cellSize.x <= 0 || cellSize.y <= 0) return;
    // Like the other bodies, rotation is ignored: position and scale only.
    Xform g = globalXform();
    float sx = std::fabs(g.scale.x) < 1e-6f ? 1e-6f : g.scale.x;
    float sy = std::fabs(g.scale.y) < 1e-6f ? 1e-6f : g.scale.y;
    float ax = (area.x - g.position.x) / sx, bx = (area.x + area.width - g.position.x) / sx;
    float ay = (area.y - g.position.y) / sy, by = (area.y + area.height - g.position.y) / sy;
    int c0 = static_cast<int>(std::floor(std::min(ax, bx) / cellSize.x)) - 1;
    int c1 = static_cast<int>(std::floor(std::max(ax, bx) / cellSize.x)) + 1;
    int r0 = static_cast<int>(std::floor(std::min(ay, by) / cellSize.y)) - 1;
    int r1 = static_cast<int>(std::floor(std::max(ay, by) / cellSize.y)) + 1;
    auto add = [&](int x, int y) {
        float wx = g.position.x + x * cellSize.x * sx, wy = g.position.y + y * cellSize.y * sy;
        float w = cellSize.x * sx, h = cellSize.y * sy;
        Rectangle r{std::min(wx, wx + w), std::min(wy, wy + h), std::fabs(w), std::fabs(h)};
        if (CheckCollisionRecs(r, area)) out.push_back(r);
    };
    double span = static_cast<double>(c1 - c0 + 1) * (r1 - r0 + 1);
    if (span > static_cast<double>(cells.size())) {
        for (auto& [pos, c] : cells)
            if (pos.first >= c0 && pos.first <= c1 && pos.second >= r0 && pos.second <= r1 && isSolid(pos.first, pos.second))
                add(pos.first, pos.second);
    } else {
        for (int y = r0; y <= r1; ++y)
            for (int x = c0; x <= c1; ++x)
                if (isSolid(x, y)) add(x, y);
    }
}

namespace {

// A point of this node's space in the world, with the transform worked out once.
struct Placer {
    raylib::Vector2 origin, xAxis, yAxis;  // where local (0,0) goes, and local (1,0) / (0,1) as steps
    explicit Placer(const Xform& g) {
        float r = g.rotation * DEG2RAD, c = std::cos(r), s = std::sin(r);
        origin = g.position;
        xAxis = raylib::Vector2(c * g.scale.x, s * g.scale.x);
        yAxis = raylib::Vector2(-s * g.scale.y, c * g.scale.y);
    }
    raylib::Vector2 at(float x, float y) const {
        return {origin.x + xAxis.x * x + yAxis.x * y, origin.y + xAxis.y * x + yAxis.y * y};
    }
};

void drawTileQuad(const raylib::Texture& tex, Rectangle src, const TileCell& cell, const Placer& p, float lx, float ly,
                  float w, float h, Color tint, bool mirrored) {
    // Texture coordinates at the tile's corners: top-left, top-right, bottom-right, bottom-left.
    float u0 = src.x / tex.width, v0 = src.y / tex.height;
    float u1 = (src.x + src.width) / tex.width, v1 = (src.y + src.height) / tex.height;
    raylib::Vector2 uv[4] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
    // Tiled's order: the diagonal flip first, then horizontal, then vertical.
    if (cell.flags & TileMap::FlipDiagonal) std::swap(uv[1], uv[3]);
    if (cell.flags & TileMap::FlipH) {
        std::swap(uv[0], uv[1]);
        std::swap(uv[2], uv[3]);
    }
    if (cell.flags & TileMap::FlipV) {
        std::swap(uv[0], uv[3]);
        std::swap(uv[1], uv[2]);
    }
    raylib::Vector2 corner[4] = {p.at(lx, ly), p.at(lx + w, ly), p.at(lx + w, ly + h), p.at(lx, ly + h)};
    // raylib's quads go top-left, bottom-left, bottom-right, top-right (the
    // other way round when the map is mirrored, or it would be culled).
    static const int order[4] = {0, 3, 2, 1}, mirroredOrder[4] = {0, 1, 2, 3};
    const int* o = mirrored ? mirroredOrder : order;
    rlCheckRenderBatchLimit(4);
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);
    for (int k = 0; k < 4; ++k) {
        rlTexCoord2f(uv[o[k]].x, uv[o[k]].y);
        rlVertex2f(corner[o[k]].x, corner[o[k]].y);
    }
    rlEnd();
    rlSetTexture(0);
}

}  // namespace

void TileMap::drawTile(const TileCell& cell, raylib::Vector2 local, Color tint) const {
    if (cell.source < 0 || cell.source >= static_cast<int>(sources.size())) return;
    const TileSource& s = sources[cell.source];
    raylib::Texture* tex = Assets::texture(s.texture);
    if (!tex) return;
    Xform g = globalXform();
    Placer p(g);
    float w = static_cast<float>(s.tileWidth), h = static_cast<float>(s.tileHeight);
    if (cell.flags & FlipDiagonal) std::swap(w, h);
    drawTileQuad(*tex, s.tileRect(cell.tile), cell, p, local.x, local.y, w, h, tint, g.scale.x * g.scale.y < 0);
}

void TileMap::draw(const DrawOptions& options) {
    if (cells.empty()) return;
    Xform g = globalXform();
    Placer p(g);
    bool mirrored = g.scale.x * g.scale.y < 0;

    // Only the cells on screen (when the view is known).
    int c0 = 0, c1 = -1, r0 = 0, r1 = -1;
    bool culled = options.view.width > 0 && options.view.height > 0 && cellSize.x > 0 && cellSize.y > 0;
    if (culled) {
        Rectangle v = options.view;
        raylib::Vector2 pts[4] = {toLocal({v.x, v.y}), toLocal({v.x + v.width, v.y}),
                                  toLocal({v.x + v.width, v.y + v.height}), toLocal({v.x, v.y + v.height})};
        float minX = pts[0].x, maxX = pts[0].x, minY = pts[0].y, maxY = pts[0].y;
        for (auto& q : pts) {
            minX = std::min(minX, q.x);
            maxX = std::max(maxX, q.x);
            minY = std::min(minY, q.y);
            maxY = std::max(maxY, q.y);
        }
        // Tiles bigger than a cell stick out of it (up and to the right, like Tiled's)
        float bigW = 0, bigH = 0;
        for (auto& s : sources) {
            bigW = std::max(bigW, static_cast<float>(std::max(s.tileWidth, s.tileHeight)));
            bigH = bigW;
        }
        c0 = static_cast<int>(std::floor(minX / cellSize.x)) - 1 - static_cast<int>(bigW / cellSize.x);
        c1 = static_cast<int>(std::floor(maxX / cellSize.x)) + 1;
        r0 = static_cast<int>(std::floor(minY / cellSize.y)) - 1;
        r1 = static_cast<int>(std::floor(maxY / cellSize.y)) + 1 + static_cast<int>(bigH / cellSize.y);
        if (static_cast<double>(c1 - c0 + 1) * (r1 - r0 + 1) > static_cast<double>(cells.size())) culled = false;
    }

    // Each source's texture, looked up once.
    std::vector<raylib::Texture*> textures;
    for (auto& s : sources) textures.push_back(Assets::texture(s.texture));

    auto drawCell = [&](int x, int y, const TileCell& c) {
        if (c.source < 0 || c.source >= static_cast<int>(sources.size()) || !textures[c.source]) {
            if (options.editor) {  // a tile whose picture is missing
                Color m = gizmoColors().missingTexture;
                drawLocalRect(*this, {x * cellSize.x, y * cellSize.y, cellSize.x, cellSize.y}, Fade(m, 0.3f), BLANK, 0);
            }
            return;
        }
        const TileSource& s = sources[c.source];
        float w = static_cast<float>(s.tileWidth), h = static_cast<float>(s.tileHeight);
        if (c.flags & FlipDiagonal) std::swap(w, h);
        // Bottom-left of the cell, like Tiled (tiles taller than a cell stick out at the top)
        float lx = x * cellSize.x, ly = (y + 1) * cellSize.y - h;
        drawTileQuad(*textures[c.source], s.tileRect(c.tile), c, p, lx, ly, w, h, modulate, mirrored);
    };
    if (culled) {
        for (int y = r0; y <= r1; ++y)
            for (int x = c0; x <= c1; ++x)
                if (auto it = cells.find({x, y}); it != cells.end()) drawCell(x, y, it->second);
    } else {
        for (auto& [pos, c] : cells) drawCell(pos.first, pos.second, c);
    }
}

// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------
json tilesToJson(const TileMap& map) {
    const auto& sources = map.sources;
    const auto& cells = map.cells;
    json j;
    json list = json::array();
    for (const TileSource& s : sources) {
        json e = {{"texture", s.texture}, {"tile_size", {s.tileWidth, s.tileHeight}}};
        if (s.margin) e["margin"] = s.margin;
        if (s.spacing) e["spacing"] = s.spacing;
        if (!s.solid.empty()) e["solid"] = std::vector<int>(s.solid.begin(), s.solid.end());
        if (!s.types.empty()) {
            json types = json::object();
            for (auto& [tile, name] : s.types) types[std::to_string(tile)] = name;
            e["types"] = types;
        }
        list.push_back(e);
    }
    j["sources"] = list;
    // Cells as one flat list of numbers, five per cell: column, row, source, tile, flips.
    std::vector<int> flat;
    flat.reserve(cells.size() * 5);
    for (auto& [pos, c] : cells) {
        flat.push_back(pos.first);
        flat.push_back(pos.second);
        flat.push_back(c.source);
        flat.push_back(c.tile);
        flat.push_back(c.flags);
    }
    j["cells"] = flat;
    return j;
}

void tilesFromJson(TileMap& map, const json& j) {
    if (!j.is_object()) return;
    auto& sources = map.sources;
    auto& cells = map.cells;
    sources.clear();
    cells.clear();
    if (j.contains("sources") && j["sources"].is_array()) {
        for (const json& e : j["sources"]) {
            TileSource s;
            s.texture = e.value("texture", std::string());
            if (e.contains("tile_size") && e["tile_size"].is_array() && e["tile_size"].size() == 2) {
                s.tileWidth = std::max(1, e["tile_size"][0].get<int>());
                s.tileHeight = std::max(1, e["tile_size"][1].get<int>());
            }
            s.margin = std::max(0, e.value("margin", 0));
            s.spacing = std::max(0, e.value("spacing", 0));
            if (e.contains("solid") && e["solid"].is_array())
                for (const json& t : e["solid"]) s.solid.insert(t.get<int>());
            if (e.contains("types") && e["types"].is_object())
                for (auto& [tile, name] : e["types"].items()) s.types[std::atoi(tile.c_str())] = name.get<std::string>();
            sources.push_back(s);
        }
    }
    if (j.contains("cells") && j["cells"].is_array()) {
        const json& flat = j["cells"];
        for (size_t i = 0; i + 4 < flat.size(); i += 5)
            cells[{flat[i].get<int>(), flat[i + 1].get<int>()}] =
                TileCell{flat[i + 2].get<int>(), flat[i + 3].get<int>(), flat[i + 4].get<int>()};
    }
}

}  // namespace s2d
