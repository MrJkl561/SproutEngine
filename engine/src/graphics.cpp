#include "sprout/graphics.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <string>

#include "internal.hpp"

namespace sprout {

namespace {

// Every picture loaded so far. raylib::Texture frees the picture by itself
// when it is removed from the map.
std::map<std::string, raylib::Texture>& textureCache() {
    static std::map<std::string, raylib::Texture> cache;
    return cache;
}

// A pink and black checkerboard, used when a picture can't be found.
std::optional<raylib::Texture>& missingTextureSlot() {
    static std::optional<raylib::Texture> texture;
    return texture;
}

Texture missingTexture() {
    auto& slot = missingTextureSlot();
    if (!slot) {
        raylib::Image checkerboard{raylib::Image::Checked(32, 32, 8, 8, raylib::Color::Magenta(), raylib::Color::Black())};
        slot.emplace(checkerboard);
    }
    return fromRaylib(*slot);
}

struct FontState {
    std::optional<raylib::Font> custom; // empty = the built-in pixel font
    std::string path;
};

FontState& fontState() {
    static FontState state;
    return state;
}

} // namespace

// --- Shapes ----------------------------------------------------------------------
void drawRect(float x, float y, float width, float height, Color color) {
    raylib::Rectangle{x, y, width, height}.Draw(toRaylib(color));
}

void drawRect(const Rect& rect, Color color) { toRaylib(rect).Draw(toRaylib(color)); }

void drawRectOutline(const Rect& rect, Color color, float thickness) {
    toRaylib(rect).DrawLines(toRaylib(color), thickness);
}

void drawRoundedRect(const Rect& rect, float cornerRadius, Color color) {
    const float shortest = std::min(rect.width, rect.height);
    if (shortest <= 0.0f) return;
    // raylib wants "roundness" from 0 to 1 instead of a radius in pixels.
    const float roundness = std::clamp(cornerRadius * 2.0f / shortest, 0.0f, 1.0f);
    toRaylib(rect).DrawRounded(roundness, 8, toRaylib(color));
}

void drawCircle(Vec2 center, float radius, Color color) { toRaylib(center).DrawCircle(radius, toRaylib(color)); }

void drawCircle(float x, float y, float radius, Color color) { drawCircle(Vec2{x, y}, radius, color); }

void drawCircleOutline(Vec2 center, float radius, Color color, float thickness) {
    // raylib-cpp has no wrapper for rings, so this one uses raylib directly.
    ::DrawRing(toRaylib(center), std::max(0.0f, radius - thickness), radius, 0.0f, 360.0f, 48, toRaylib(color));
}

void drawLine(Vec2 from, Vec2 to, Color color, float thickness) {
    toRaylib(from).DrawLine(toRaylib(to), thickness, toRaylib(color));
}

void drawTriangle(Vec2 a, Vec2 b, Vec2 c, Color color) {
    // raylib only draws triangles whose corners go counter-clockwise, so fix the order.
    const float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross > 0.0f) std::swap(b, c);
    ::DrawTriangle(toRaylib(a), toRaylib(b), toRaylib(c), toRaylib(color));
}

void drawPolygon(Vec2 center, int sides, float radius, Color color, float rotation) {
    toRaylib(center).DrawPoly(std::max(3, sides), radius, rotation, toRaylib(color));
}

// --- Text ------------------------------------------------------------------------
namespace detail {
raylib::FontUnmanaged currentFont() {
    FontState& state = fontState();
    // A default-made FontUnmanaged is raylib's built-in font.
    return state.custom ? raylib::FontUnmanaged{*state.custom} : raylib::FontUnmanaged{};
}

float textSpacing(float size) {
    // The built-in pixel font needs a little gap between letters; real fonts don't.
    return fontState().custom ? size / 32.0f : size / 10.0f;
}

void unloadFonts() { fontState() = FontState{}; }
} // namespace detail

void drawText(const std::string& text, float x, float y, float size, Color color) {
    detail::currentFont().DrawText(text, raylib::Vector2{x, y}, size, detail::textSpacing(size), toRaylib(color));
}

void drawText(const std::string& text, Vec2 position, float size, Color color) {
    drawText(text, position.x, position.y, size, color);
}

void drawTextCentered(const std::string& text, Vec2 center, float size, Color color) {
    const raylib::Vector2 measured = detail::currentFont().MeasureText(text, size, detail::textSpacing(size));
    drawText(text, center.x - measured.x / 2.0f, center.y - measured.y / 2.0f, size, color);
}

float measureText(const std::string& text, float size) {
    if (!detail::windowIsOpen()) return 0.0f;
    return detail::currentFont().MeasureText(text, size, detail::textSpacing(size)).x;
}

void drawFPS(float x, float y) { raylib::Window::DrawFPS(static_cast<int>(x), static_cast<int>(y)); }

void useFont(const std::string& path, int quality) {
    FontState& state = fontState();
    if (state.custom && state.path == path) return;
    if (!detail::windowIsOpen()) {
        detail::warn("useFont(\"" + path + "\") was called before the Game window was created.");
        return;
    }
    if (!raylib::FileExists(path)) {
        detail::warn("couldn't find the font \"" + path + "\". Check the spelling and that it is in your "
                     "game's assets folder.");
        return;
    }
    try {
        raylib::Font font{path, quality};
        raylib::TextureUnmanaged{font.texture}.SetFilter(TEXTURE_FILTER_BILINEAR); // smooth edges for real fonts
        state.custom.emplace(std::move(font));
        state.path = path;
    } catch (const raylib::RaylibException&) {
        detail::warn("couldn't load the font \"" + path + "\". Is the file in your assets folder?");
    }
}

void useDefaultFont() { detail::unloadFonts(); }

// --- Textures --------------------------------------------------------------------
Texture loadTexture(const std::string& path) {
    auto& cache = textureCache();
    if (auto it = cache.find(path); it != cache.end()) return fromRaylib(it->second);

    if (!detail::windowIsOpen()) {
        detail::warn("loadTexture(\"" + path + "\") was called before the Game window was created.");
        return {};
    }
    if (!raylib::FileExists(path)) {
        detail::warn("couldn't find the picture \"" + path + "\". Check the spelling and that it is in your "
                     "game's assets folder. Using a checkerboard instead.");
        return missingTexture();
    }
    try {
        auto [it, added] = cache.emplace(path, raylib::Texture{path});
        return fromRaylib(it->second);
    } catch (const raylib::RaylibException&) {
        detail::warn("couldn't load the picture \"" + path + "\". Is it a .png, .jpg or .bmp file?");
        return missingTexture();
    }
}

void drawTexture(const Texture& texture, Vec2 position, Color tint) {
    toRaylib(texture).Draw(toRaylib(position), toRaylib(tint));
}

void drawTextureCentered(const Texture& texture, Vec2 center, float scale, float rotation, Color tint) {
    const float width = static_cast<float>(texture.width) * scale;
    const float height = static_cast<float>(texture.height) * scale;
    drawTexturePart(texture, Rect{0, 0, texture.width, texture.height}, Rect{center.x, center.y, width, height},
                    Vec2{width / 2.0f, height / 2.0f}, rotation, tint);
}

void drawTexturePart(const Texture& texture, const Rect& source, const Rect& destination, Vec2 origin,
                     float rotation, Color tint) {
    toRaylib(texture).Draw(toRaylib(source), toRaylib(destination), toRaylib(origin), rotation, toRaylib(tint));
}

void setTextureSmooth(const Texture& texture, bool smooth) {
    toRaylib(texture).SetFilter(smooth ? TEXTURE_FILTER_BILINEAR : TEXTURE_FILTER_POINT);
}

namespace detail {
void unloadAllTextures() {
    textureCache().clear(); // raylib::Texture unloads itself
    missingTextureSlot().reset();
}
} // namespace detail

} // namespace sprout
