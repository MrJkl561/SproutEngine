// Sprout Editor: a small Godot-style 2D editor built on raylib, raylib-cpp and raygui.
//
//   sprout_editor [project folder]
//   sprout_editor --check-scripts    load every C++ script without opening a
//                                    window, then exit (used by CI)
//   sprout_editor --export linux,web [output folder]
//                                    export the game without opening a window
//
// With no folder it opens editor/project/ from the source tree, and new C++
// scripts are written to editor/scripts/ so the next build compiles them in.
#include <raylib-cpp.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "editor.hpp"
#include "export.hpp"
#include "hot_reload.hpp"
#include "project.hpp"
#include "raygui_include.hpp"
#include "script.hpp"
#include "ui_scale.hpp"

#ifndef SPROUT_EDITOR_PROJECT_DIR
#define SPROUT_EDITOR_PROJECT_DIR "project"
#endif
#ifndef SPROUT_EDITOR_SCRIPTS_DIR
#define SPROUT_EDITOR_SCRIPTS_DIR "scripts"
#endif

namespace {

// Loads the scripts (the hot reload library, if this build uses one) and makes
// one of each, without a window. Proves the library loads and links on this
// system: on Windows that's the part most likely to go wrong.
int checkScripts() {
    s2d::ScriptReloader reloader(SPROUT_EDITOR_SCRIPTS_DIR);
    if (s2d::ScriptReloader::supported()) {
        std::string error;
        if (!reloader.load(error)) {
            std::printf("FAILED to load the scripts library: %s\n", error.c_str());
            return 1;
        }
        std::printf("Loaded the scripts library (hot reloading is on).\n");
    } else {
        std::printf("Scripts are compiled into the editor (hot reloading is off).\n");
    }
    auto names = s2d::ScriptRegistry::names();
    for (const std::string& name : names) {
        auto script = s2d::ScriptRegistry::create(name);
        std::printf("  %s: %s\n", name.c_str(), script ? "ok" : "FAILED");
        if (!script) return 1;
    }
    std::printf("%d scripts OK\n", static_cast<int>(names.size()));
    reloader.unload();
    return 0;
}

// Exports the game from the command line, printing progress. Exit code 0 if
// every platform worked.
int exportGame(const std::string& targets, const std::string& outDir) {
    std::filesystem::path project = SPROUT_EDITOR_PROJECT_DIR;
    s2d::ProjectSettings settings = s2d::ProjectSettings::load(project);
    s2d::Exporter exporter({SPROUT_EDITOR_DIR, project, SPROUT_EDITOR_SCRIPTS_DIR,
                            std::filesystem::path(SPROUT_EDITOR_BUILD_DIR) / "exports", SPROUT_EDITOR_DEPS_DIR,
                            SPROUT_EDITOR_CMAKE});
    std::vector<std::string> ids;
    for (size_t start = 0; start <= targets.size();) {
        size_t comma = targets.find(',', start);
        ids.push_back(targets.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    std::filesystem::path out = outDir.empty() ? std::filesystem::path(SPROUT_EDITOR_DIR) / "exports" : std::filesystem::path(outDir);
    bool started = exporter.start(ids, settings.name, out);
    bool failed = !started;
    auto print = [&] {
        for (auto& line : exporter.takeLines()) {
            std::printf("%s\n", line.text.c_str());
            if (line.kind == 2 && line.text.rfind("Export finished", 0) == 0) failed = true;
        }
        std::fflush(stdout);
    };
    while (exporter.busy()) {
        print();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    exporter.wait();
    print();
    return failed ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--check-scripts") return checkScripts();
    if (argc > 2 && std::string(argv[1]) == "--export") return exportGame(argv[2], argc > 3 ? argv[3] : "");
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
