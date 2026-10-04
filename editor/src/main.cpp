// Sprout Editor: a small Godot-style 2D editor built on raylib, raylib-cpp and raygui.
//
//   sprout_editor [project folder]
//
// With no folder it opens editor/project/ from the source tree, and new C++
// scripts are written to editor/scripts/ so the next build compiles them in.
#include <raylib-cpp.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "editor.hpp"
#include "raygui_include.hpp"
#include "ui_scale.hpp"

#ifndef SPROUT_EDITOR_PROJECT_DIR
#define SPROUT_EDITOR_PROJECT_DIR "project"
#endif
#ifndef SPROUT_EDITOR_SCRIPTS_DIR
#define SPROUT_EDITOR_SCRIPTS_DIR "scripts"
#endif

int main(int argc, char** argv) {
    std::filesystem::path project = argc > 1 ? argv[1] : SPROUT_EDITOR_PROJECT_DIR;
    std::filesystem::path scripts = SPROUT_EDITOR_SCRIPTS_DIR;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    raylib::Window window(1280, 720, "Sprout Editor");

    // Size the window for the display: 1600x900 UI pixels at the display's
    // scale (e.g. 150% on many laptops), but never bigger than the screen.
    float scale = s2d::systemUiScale();
    int monitor = GetCurrentMonitor();
    int mw = GetMonitorWidth(monitor), mh = GetMonitorHeight(monitor);
    if (mw > 0 && mh > 0) {
        int w = std::min(static_cast<int>(1600 * scale), mw * 9 / 10);
        int h = std::min(static_cast<int>(900 * scale), mh * 85 / 100);
        Vector2 mpos = GetMonitorPosition(monitor);
        window.SetSize(w, h);
        window.SetPosition(static_cast<int>(mpos.x) + (mw - w) / 2, static_cast<int>(mpos.y) + (mh - h) / 2);
        window.SetMinSize(std::min(static_cast<int>(900 * scale), w), std::min(static_cast<int>(560 * scale), h));
        // A small screen can't fit the whole layout at full scale: shrink it.
        while (scale > 1.0f && (w / scale < 1000 || h / scale < 560)) scale -= 0.25f;
    }
    window.SetTargetFPS(60);
    SetExitKey(KEY_NULL);  // Esc stops the game instead of closing the editor
    s2d::loadEditorStyle();

    // SPROUT_TEST_FRAMES=N quits after N frames (used by CI to check the editor starts).
    // SPROUT_EDITOR_SCREENSHOT=file.png saves a picture of the last frame.
    const char* framesEnv = std::getenv("SPROUT_TEST_FRAMES");
    long testFrames = framesEnv ? std::strtol(framesEnv, nullptr, 10) : 0;
    const char* screenshot = std::getenv("SPROUT_EDITOR_SCREENSHOT");

    {
        s2d::Editor editor(project, scripts);
        editor.setUiScale(scale);
        for (long frame = 1; !window.ShouldClose(); ++frame) {
            if (screenshot && testFrames > 0 && frame == testFrames) editor.screenshotPath = screenshot;
            editor.frame();
            if (testFrames > 0 && frame >= testFrames) break;
        }
    }  // the editor frees its textures before the window closes
    return 0;
}
