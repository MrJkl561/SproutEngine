#pragma once
// Private helpers shared by the engine's .cpp files. Not part of the public API.
//
// The engine is built on raylib-cpp (https://github.com/RobLoach/raylib-cpp),
// the C++ wrapper for raylib. Note: raylib has its own global types called
// Color, Texture, Camera, etc. Inside namespace sprout, plain `Color` means
// sprout::Color, `raylib::Color` is raylib-cpp's and `::Color` is raylib's C struct.
#include <string>

#include "sprout/raylib_interop.hpp"

namespace sprout {
class Scene;
}

namespace sprout::detail {

/// Prints a friendly message to the console.
void warn(const std::string& message);

/// True once the window is open (lots of raylib functions crash without one).
bool windowIsOpen();

/// The scene that is running, or nullptr.
Scene* activeScene();

// Called by Game every frame / at startup / at shutdown.
void beginInputFrame();
void updateAudio();
void initAudio();
void shutdownAudio();
void unloadAllTextures();
void unloadFonts();
void initUI();
/// Called after each frame is drawn: turns the exit key off while a textBox is
/// being typed in (so Esc doesn't quit mid-name), and back on otherwise.
void endUIFrame(int exitKey);

// Text drawing with the current font (see useFont()).
raylib::FontUnmanaged currentFont();
float textSpacing(float size);

} // namespace sprout::detail
