// Sprout Editor: a small Godot-style 2D editor built on raylib, raylib-cpp and raygui.
//
//   sprout_editor [project folder]
//
// With no folder it opens editor/project/ from the source tree, and new C++
// scripts are written to editor/scripts/ so the next build compiles them in.
#include <raylib-cpp.hpp>

#include <cstdlib>
#include <filesystem>
#include <string>

#include "editor.hpp"
#include "raygui_include.hpp"

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
    raylib::Window window(1600, 900, "Sprout Editor");
    window.SetMinSize(1100, 650);
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
        for (long frame = 1; !window.ShouldClose(); ++frame) {
            if (screenshot && testFrames > 0 && frame == testFrames) editor.screenshotPath = screenshot;
            editor.frame();
            if (testFrames > 0 && frame >= testFrames) break;
        }
    }  // the editor frees its textures before the window closes
    return 0;
}
