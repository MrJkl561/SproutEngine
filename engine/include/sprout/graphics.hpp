#pragma once
// =============================================================================
//  sprout/graphics.hpp - drawing shapes, text and pictures
//
//  Call these from your scene's draw() or drawUI() functions.
//
//    drawRect(100, 100, 50, 80, Color::Red);
//    drawCircle({400, 300}, 25, Color::Yellow);
//    drawText("Score: " + std::to_string(score), 10, 10, 30, Color::White);
//
//  Things drawn later appear ON TOP of things drawn earlier.
// =============================================================================
#include <string>

#include "sprout/color.hpp"
#include "sprout/math.hpp"

namespace sprout {

// --- Shapes ------------------------------------------------------------------
/// A filled rectangle. x and y are the top-left corner.
void drawRect(float x, float y, float width, float height, Color color);
void drawRect(const Rect& rect, Color color);
/// Just the border of a rectangle.
void drawRectOutline(const Rect& rect, Color color, float thickness = 2.0f);
/// A rectangle with rounded corners.
void drawRoundedRect(const Rect& rect, float cornerRadius, Color color);

/// A filled circle. The position is the MIDDLE of the circle.
void drawCircle(Vec2 center, float radius, Color color);
void drawCircle(float x, float y, float radius, Color color);
/// Just the border of a circle.
void drawCircleOutline(Vec2 center, float radius, Color color, float thickness = 2.0f);

/// A line from one point to another.
void drawLine(Vec2 from, Vec2 to, Color color, float thickness = 2.0f);
/// A filled triangle. The corners can be in any order.
void drawTriangle(Vec2 a, Vec2 b, Vec2 c, Color color);
/// A regular shape: 3 sides = triangle, 5 = pentagon, 6 = hexagon...
void drawPolygon(Vec2 center, int sides, float radius, Color color, float rotation = 0.0f);

// --- Text --------------------------------------------------------------------
/// Draws text. x and y are the top-left corner of the text.
void drawText(const std::string& text, float x, float y, float size, Color color = Color::White);
void drawText(const std::string& text, Vec2 position, float size, Color color = Color::White);
/// Draws text with its MIDDLE at `center`. Great for titles and buttons.
void drawTextCentered(const std::string& text, Vec2 center, float size, Color color = Color::White);
/// How wide some text will be, in pixels.
float measureText(const std::string& text, float size);
/// Shows the frames-per-second counter.
void drawFPS(float x = 10.0f, float y = 10.0f);
/// Uses a font file (.ttf or .otf) for all text from now on.
/// `quality` is the size the letters are made at; bigger = sharper large text.
void useFont(const std::string& path, int quality = 64);
/// Goes back to the built-in pixel font.
void useDefaultFont();

// --- Pictures (textures) -----------------------------------------------------
/// A picture loaded into the graphics card. Get one with loadTexture().
/// It's just a small "ticket" - copying it is cheap and doesn't copy the picture.
struct Texture {
    unsigned int id = 0;
    int width = 0;
    int height = 0;
    int mipmaps = 0;
    int format = 0;

    bool isValid() const { return id != 0; }
    Vec2 size() const { return {width, height}; }
};

/// Loads a picture (.png, .jpg, .bmp, ...). Loading the same file twice is free -
/// you get the same picture back. If the file is missing you'll get a
/// pink-and-black checkerboard and a message in the console.
Texture loadTexture(const std::string& path);
/// Draws a whole picture with its top-left corner at `position`.
void drawTexture(const Texture& texture, Vec2 position, Color tint = Color::White);
/// Draws a picture with its MIDDLE at `center`, optionally scaled (2 = twice as big)
/// and rotated (in degrees).
void drawTextureCentered(const Texture& texture, Vec2 center, float scale = 1.0f, float rotation = 0.0f,
                         Color tint = Color::White);
/// Advanced: draws part of a picture (`source`) stretched into `destination`.
/// `origin` is the point (inside destination) that the picture rotates around.
void drawTexturePart(const Texture& texture, const Rect& source, const Rect& destination,
                     Vec2 origin = {}, float rotation = 0.0f, Color tint = Color::White);
/// Pixel art looks crisp by default. Turn smoothing on for high-resolution art.
void setTextureSmooth(const Texture& texture, bool smooth);

} // namespace sprout
