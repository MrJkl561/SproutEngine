#include "sprout/game.hpp"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "internal.hpp"

#include <rlgl.h>

namespace sprout {

namespace {

struct GameState {
    std::optional<raylib::Window> window; // raylib::Window closes itself when reset
    std::unique_ptr<Scene> scene;
    detail::SceneFactory currentFactory;
    detail::SceneFactory nextFactory;
    bool quitRequested = false;
    float dt = 0.0f;
    double realTime = 0.0;
    double gameClock = 0.0;
    int exitKey = KEY_ESCAPE;
    std::string pendingScreenshot; // taken at the end of the frame, once everything is drawn
};

GameState& state() {
    static GameState s;
    return s;
}

// Environment variables used by the engine's automatic tests:
//   SPROUT_TEST_FRAMES=120          quit after 120 frames
//   SPROUT_TEST_SCREENSHOT=out.png  save a screenshot just before quitting
int testFrameLimit() {
    const char* value = std::getenv("SPROUT_TEST_FRAMES");
    return value ? std::atoi(value) : 0;
}

void saveScreenshot(const std::string& path) {
    rlDrawRenderBatchActive(); // make sure everything drawn so far is really on screen
    raylib::Image image{raylib::Image::LoadFromScreen()};
    try {
        image.Export(path);
    } catch (const raylib::RaylibException&) {
        detail::warn("couldn't save the screenshot \"" + path + "\".");
    }
}

// Closes everything down: scenes, sounds, pictures, fonts and the window.
void shutdownEverything() {
    GameState& s = state();
    s.scene.reset();
    s.currentFactory = nullptr;
    s.nextFactory = nullptr;
    if (s.window) {
        detail::shutdownAudio();
        detail::unloadAllTextures();
        detail::unloadFonts();
        s.window.reset(); // closes the window
    }
    s.quitRequested = false;
    s.dt = 0.0f;
    s.realTime = 0.0;
    s.gameClock = 0.0;
    s.exitKey = KEY_ESCAPE;
    s.pendingScreenshot.clear();
}

void switchScene(detail::SceneFactory factory) {
    GameState& s = state();
    s.scene.reset(); // the old scene goes away first
    s.currentFactory = std::move(factory);
    s.scene = s.currentFactory();
    s.scene->runStart();
}

} // namespace

// --- Engine helpers ----------------------------------------------------------------
namespace detail {
void warn(const std::string& message) { std::cerr << "[Sprout] Warning: " << message << std::endl; }

bool windowIsOpen() { return state().window.has_value(); }

Scene* activeScene() { return state().scene.get(); }

bool hasScene() { return state().scene != nullptr; }

double gameClock() { return state().gameClock; }

void advanceGameClock(double seconds) { state().gameClock += seconds; }

void requestSceneChange(SceneFactory factory) { state().nextFactory = std::move(factory); }
} // namespace detail

// --- Game --------------------------------------------------------------------------
Game::Game(const std::string& title, int width, int height) {
    GameState& s = state();
    if (s.window) {
        throw std::logic_error("Only one Game can exist at a time.");
    }
    try {
        // Smooth edges + no tearing, and only print raylib messages that matter.
        s.window.emplace(width, height, title, FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT, LOG_WARNING);
    } catch (const raylib::RaylibException&) {
        throw std::runtime_error("Sprout couldn't open a window. Is your graphics driver installed?");
    }

    // Run from the folder the game is in, so "assets/player.png" is always found.
    raylib::ChangeDirectory(::GetApplicationDirectory());

    s.window->SetTargetFPS(60);
    detail::initAudio();
    detail::initUI();
}

Game::~Game() { shutdownEverything(); }

void Game::runWithFactory(detail::SceneFactory factory) {
    GameState& s = state();
    const int frameLimit = testFrameLimit();
    int frame = 0;

    try {
        switchScene(std::move(factory));

        raylib::Window& window = *s.window;
        while (!s.quitRequested && !window.ShouldClose()) {
            // Never step more than 1/10 of a second at once (e.g. after dragging the window),
            // otherwise things could fly through walls.
            s.dt = std::min(window.GetFrameTime(), 0.1f);
            s.realTime += s.dt;
            if (!s.scene->paused) s.gameClock += s.dt;

            detail::beginInputFrame();
            detail::updateAudio();
            s.scene->runUpdate(s.dt);

            window.BeginDrawing();
            s.scene->runDraw();
            if (!s.pendingScreenshot.empty()) {
                saveScreenshot(s.pendingScreenshot);
                s.pendingScreenshot.clear();
            }
            detail::endUIFrame(s.exitKey);
            ++frame;
            const bool lastTestFrame = frameLimit > 0 && frame >= frameLimit;
            if (lastTestFrame) {
                if (const char* shot = std::getenv("SPROUT_TEST_SCREENSHOT")) saveScreenshot(shot);
            }
            window.EndDrawing();

            if (lastTestFrame) s.quitRequested = true;
            if (s.nextFactory) {
                detail::SceneFactory next = std::move(s.nextFactory);
                s.nextFactory = nullptr;
                switchScene(std::move(next));
            }
        }
    } catch (const std::exception& error) {
        // Show a readable message instead of a scary crash, then close cleanly.
        std::cerr << "\n[Sprout] Your game stopped because of an error:\n    " << error.what() << "\n" << std::endl;
        shutdownEverything();
        std::exit(EXIT_FAILURE);
    }
    s.scene.reset();
}

// --- Scenes ------------------------------------------------------------------------
void restartScene() {
    if (state().currentFactory) state().nextFactory = state().currentFactory;
}

Scene& currentScene() {
    if (!state().scene) throw std::logic_error("currentScene() was called while no scene is running.");
    return *state().scene;
}

void quitGame() { state().quitRequested = true; }

// --- Window ------------------------------------------------------------------------
int screenWidth() { return detail::windowIsOpen() ? raylib::Window::GetWidth() : 0; }
int screenHeight() { return detail::windowIsOpen() ? raylib::Window::GetHeight() : 0; }
Vec2 screenSize() { return Vec2{screenWidth(), screenHeight()}; }
Vec2 screenCenter() { return screenSize() / 2.0f; }

void setWindowTitle(const std::string& title) {
    if (detail::windowIsOpen()) state().window->SetTitle(title);
}

void setResizable(bool resizable) {
    if (!detail::windowIsOpen()) return;
    if (resizable) state().window->SetState(FLAG_WINDOW_RESIZABLE);
    else state().window->ClearState(FLAG_WINDOW_RESIZABLE);
}

void setFullscreen(bool fullscreen) {
    // "Borderless windowed" fills the screen without changing the monitor's resolution.
    if (detail::windowIsOpen() && fullscreen != isFullscreen()) state().window->ToggleBorderless();
}

bool isFullscreen() { return detail::windowIsOpen() && raylib::Window::IsState(FLAG_BORDERLESS_WINDOWED_MODE); }

void toggleFullscreen() { setFullscreen(!isFullscreen()); }

void showCursor() {
    if (detail::windowIsOpen()) raylib::Window::ShowCursor();
}

void hideCursor() {
    if (detail::windowIsOpen()) raylib::Window::HideCursor();
}

void setExitKey(Key key) {
    state().exitKey = static_cast<int>(key);
    if (detail::windowIsOpen()) raylib::Window::SetExitKey(state().exitKey);
}

void takeScreenshot(const std::string& path) {
    // Wait until the frame is completely drawn, so it works from update() too.
    if (detail::windowIsOpen()) state().pendingScreenshot = path;
}

// --- Time --------------------------------------------------------------------------
float deltaTime() { return state().dt; }
float timeSinceStart() { return static_cast<float>(state().realTime); }
int fps() { return detail::windowIsOpen() ? raylib::Window::GetFPS() : 0; }
void setTargetFPS(int targetFps) {
    if (detail::windowIsOpen()) state().window->SetTargetFPS(targetFps);
}

} // namespace sprout
