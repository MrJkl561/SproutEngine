#include "sprout/graphics.hpp"

#include <algorithm>
#include <map>
#include <string>

#include "internal.hpp"

namespace sprout {

namespace {

std::map<std::string, ::Texture2D>& textureCache() {
    static std::map<std::string, ::Texture2D> cache;
    return cache;
}

struct FontState {
    ::Font font{};
    bool custom = false;
    std::string path;
};

FontState& fontState() {
    static FontState state;
    return state;
}

// A pink and black checkerboard, used when a picture can't be found.
::Texture2D gMissingTexture{};

::Texture2D missingTexture() {
    if (gMissingTexture.id == 0) {
        ::Image image = GenImageChecked(32, 32, 8, 8, ::Color{255, 0, 255, 255}, ::Color{0, 0, 0, 255});
        gMissingTexture = LoadTextureFromImage(image);
        UnloadImage(image);
    }
    return gMissingTexture;
}

} // namespace

// --- Shapes ----------------------------------------------------------------------
void drawRect(float x, float y, float width, float height, Color color) {
    DrawRectangleRec(::Rectangle{x, y, width, height}, detail::toRay(color));
}

void drawRect(const Rect& rect, Color color) { DrawRectangleRec(detail::toRay(rect), detail::toRay(color)); }

void drawRectOutline(const Rect& rect, Color color, float thickness) {
    DrawRectangleLinesEx(detail::toRay(rect), thickness, detail::toRay(color));
}

void drawRoundedRect(const Rect& rect, float cornerRadius, Color color) {
    const float shortest = std::min(rect.width, rect.height);
    if (shortest <= 0.0f) return;
    // raylib wants "roundness" from 0 to 1 instead of a radius in pixels.
    const float roundness = std::clamp(cornerRadius * 2.0f / shortest, 0.0f, 1.0f);
    DrawRectangleRounded(detail::toRay(rect), roundness, 8, detail::toRay(color));
}

void drawCircle(Vec2 center, float radius, Color color) {
    DrawCircleV(detail::toRay(center), radius, detail::toRay(color));
}

void drawCircle(float x, float y, float radius, Color color) { drawCircle(Vec2{x, y}, radius, color); }

void drawCircleOutline(Vec2 center, float radius, Color color, float thickness) {
    DrawRing(detail::toRay(center), std::max(0.0f, radius - thickness), radius, 0.0f, 360.0f, 48,
             detail::toRay(color));
}

void drawLine(Vec2 from, Vec2 to, Color color, float thickness) {
    DrawLineEx(detail::toRay(from), detail::toRay(to), thickness, detail::toRay(color));
}

void drawTriangle(Vec2 a, Vec2 b, Vec2 c, Color color) {
    // raylib only draws triangles whose corners go counter-clockwise, so fix the order.
    const float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross > 0.0f) std::swap(b, c);
    DrawTriangle(detail::toRay(a), detail::toRay(b), detail::toRay(c), detail::toRay(color));
}

void drawPolygon(Vec2 center, int sides, float radius, Color color, float rotation) {
    DrawPoly(detail::toRay(center), std::max(3, sides), radius, rotation, detail::toRay(color));
}

// --- Text ------------------------------------------------------------------------
namespace detail {
::Font currentFont() {
    FontState& state = fontState();
    return state.custom ? state.font : GetFontDefault();
}

float textSpacing(float size) {
    // The built-in pixel font needs a little gap between letters; real fonts don't.
    return fontState().custom ? size / 32.0f : size / 10.0f;
}

void unloadFonts() {
    FontState& state = fontState();
    if (state.custom) UnloadFont(state.font);
    state = FontState{};
}
} // namespace detail

void drawText(const std::string& text, float x, float y, float size, Color color) {
    DrawTextEx(detail::currentFont(), text.c_str(), ::Vector2{x, y}, size, detail::textSpacing(size),
               detail::toRay(color));
}

void drawText(const std::string& text, Vec2 position, float size, Color color) {
    drawText(text, position.x, position.y, size, color);
}

void drawTextCentered(const std::string& text, Vec2 center, float size, Color color) {
    const ::Vector2 measured = MeasureTextEx(detail::currentFont(), text.c_str(), size, detail::textSpacing(size));
    drawText(text, center.x - measured.x / 2.0f, center.y - measured.y / 2.0f, size, color);
}

float measureText(const std::string& text, float size) {
    if (!detail::windowIsOpen()) return 0.0f;
    return MeasureTextEx(detail::currentFont(), text.c_str(), size, detail::textSpacing(size)).x;
}

void drawFPS(float x, float y) { DrawFPS(static_cast<int>(x), static_cast<int>(y)); }

void useFont(const std::string& path, int quality) {
    FontState& state = fontState();
    if (state.custom && state.path == path) return;
    ::Font font = LoadFontEx(path.c_str(), quality, nullptr, 0);
    if (font.texture.id == 0 || font.texture.id == GetFontDefault().texture.id) {
        detail::warn("couldn't load the font \"" + path + "\". Is the file in your assets folder?");
        return;
    }
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    detail::unloadFonts();
    state.font = font;
    state.custom = true;
    state.path = path;
}

void useDefaultFont() { detail::unloadFonts(); }

// --- Textures --------------------------------------------------------------------
Texture loadTexture(const std::string& path) {
    auto& cache = textureCache();
    if (auto it = cache.find(path); it != cache.end()) return detail::fromRay(it->second);

    if (!detail::windowIsOpen()) {
        detail::warn("loadTexture(\"" + path + "\") was called before the Game window was created.");
        return {};
    }
    if (!FileExists(path.c_str())) {
        detail::warn("couldn't find the picture \"" + path + "\". Check the spelling and that it is in your "
                     "game's assets folder. Using a checkerboard instead.");
        return detail::fromRay(missingTexture());
    }
    ::Texture2D texture = LoadTexture(path.c_str());
    if (texture.id == 0) {
        detail::warn("couldn't load the picture \"" + path + "\". Is it a .png, .jpg or .bmp file?");
        return detail::fromRay(missingTexture());
    }
    cache[path] = texture;
    return detail::fromRay(texture);
}

void drawTexture(const Texture& texture, Vec2 position, Color tint) {
    DrawTextureV(detail::toRay(texture), detail::toRay(position), detail::toRay(tint));
}

void drawTextureCentered(const Texture& texture, Vec2 center, float scale, float rotation, Color tint) {
    const float width = static_cast<float>(texture.width) * scale;
    const float height = static_cast<float>(texture.height) * scale;
    drawTexturePart(texture, Rect{0, 0, texture.width, texture.height}, Rect{center.x, center.y, width, height},
                    Vec2{width / 2.0f, height / 2.0f}, rotation, tint);
}

void drawTexturePart(const Texture& texture, const Rect& source, const Rect& destination, Vec2 origin,
                     float rotation, Color tint) {
    DrawTexturePro(detail::toRay(texture), detail::toRay(source), detail::toRay(destination), detail::toRay(origin),
                   rotation, detail::toRay(tint));
}

void setTextureSmooth(const Texture& texture, bool smooth) {
    SetTextureFilter(detail::toRay(texture), smooth ? TEXTURE_FILTER_BILINEAR : TEXTURE_FILTER_POINT);
}

namespace detail {
void unloadAllTextures() {
    for (auto& [path, texture] : textureCache()) UnloadTexture(texture);
    textureCache().clear();
    if (gMissingTexture.id != 0) UnloadTexture(gMissingTexture);
    gMissingTexture = ::Texture2D{};
}
} // namespace detail

} // namespace sprout
