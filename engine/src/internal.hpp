#pragma once
// Private helpers shared by the engine's .cpp files. Not part of the public API.
//
// Note: raylib has its own global types called Color, Texture, Camera, etc.
// Inside namespace sprout, plain `Color` means sprout::Color and `::Color`
// means raylib's.
#include <raylib.h>

#include <string>

#include "sprout/color.hpp"
#include "sprout/graphics.hpp"
#include "sprout/math.hpp"

namespace sprout {
class Scene;
}

namespace sprout::detail {

inline ::Vector2 toRay(Vec2 v) { return ::Vector2{v.x, v.y}; }
inline Vec2 fromRay(::Vector2 v) { return Vec2{v.x, v.y}; }
inline ::Rectangle toRay(const Rect& r) { return ::Rectangle{r.x, r.y, r.width, r.height}; }
inline ::Color toRay(Color c) { return ::Color{c.r, c.g, c.b, c.a}; }
inline ::Texture2D toRay(const Texture& t) { return ::Texture2D{t.id, t.width, t.height, t.mipmaps, t.format}; }
inline Texture fromRay(const ::Texture2D& t) { return Texture{t.id, t.width, t.height, t.mipmaps, t.format}; }

/// Prints a friendly message to the console.
void warn(const std::string& message);

/// True once the window is open (lots of raylib functions crash without one).
bool windowIsOpen();

/// The scene that is running, or nullptr.
Scene* activeScene();

// Called by Game every frame / at shutdown.
void beginInputFrame();
void updateAudio();
void initAudio();
void shutdownAudio();
void unloadAllTextures();
void unloadFonts();
void initUI();

// Text drawing with the current font (see useFont()).
::Font currentFont();
float textSpacing(float size);

} // namespace sprout::detail
