// Sprout Editor: a small Godot-style 2D editor built on raylib, raylib-cpp and raygui.
//
//   sprout_editor [project folder]          open a project (default: the one open last time)
//   sprout_editor [--project folder] --check-scripts
//                                           build and load the project's C++ scripts without
//                                           opening a window, then exit (used by CI)
//   sprout_editor [--project folder] --export linux,web [output folder]
//                                           export the game without opening a window
//
// The command-line tools use the demo project (editor/project/) unless --project says otherwise.
#include <raylib-cpp.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "editor.hpp"
#include "export.hpp"
#include "hot_reload.hpp"
#include "project.hpp"
#include "projects.hpp"
#include "raygui_include.hpp"
#include "script.hpp"
#include "ui_scale.hpp"

#ifndef SPROUT_EDITOR_PROJECT_DIR
#define SPROUT_EDITOR_PROJECT_DIR "project"
#endif

namespace fs = std::filesystem;

namespace {

// Loads the project's scripts (building the hot reload library first, if this
// build uses one) and makes one of each, without a window. Proves the library
// builds, loads and links on this system.
int checkScripts(const fs::path& project) {
    s2d::ScriptReloader reloader(project / "scripts", s2d::projects::workDir(project, SPROUT_EDITOR_BUILD_DIR) / "scripts");
    if (s2d::ScriptReloader::supported()) {
        std::string output;
        if (!reloader.buildNow(output)) {
            std::printf("%s\nFAILED to build the scripts library.\n", output.c_str());
            return 1;
        }
        std::string error;
        if (!reloader.load(error)) {
            std::printf("FAILED to load the scripts library: %s\n", error.c_str());
            return 1;
        }
        std::printf("Built and loaded the scripts library (hot reloading is on).\n");
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
int exportGame(const fs::path& project, const std::string& targets, const std::string& outDir) {
    s2d::ProjectSettings settings = s2d::ProjectSettings::load(project);
    s2d::Exporter exporter({SPROUT_EDITOR_DIR, project, project / "scripts",
                            s2d::projects::workDir(project, SPROUT_EDITOR_BUILD_DIR) / "exports",
                            SPROUT_EDITOR_DEPS_DIR, SPROUT_EDITOR_CMAKE});
    std::vector<std::string> ids;
    for (size_t start = 0; start <= targets.size();) {
        size_t comma = targets.find(',', start);
        ids.push_back(targets.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    fs::path out = outDir.empty() ? project / "exports" : fs::path(outDir);
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

// The project to open: the one named on the command line, else the one open
// last time, else the demo project (else a new one in ~/SproutProjects).
fs::path startupProject(const std::string& fromCommandLine) {
    if (!fromCommandLine.empty()) return fromCommandLine;
    s2d::projects::Recent recent = s2d::projects::loadRecent();
    if (!recent.last.empty() && s2d::projects::isProject(recent.last)) return recent.last;
    if (s2d::projects::isProject(SPROUT_EDITOR_PROJECT_DIR)) return SPROUT_EDITOR_PROJECT_DIR;
    fs::path fresh = s2d::projects::defaultProjectsFolder() / "My Game";
    std::string error;
    if (!s2d::projects::isProject(fresh)) s2d::projects::createProject(fresh, "My Game", error);
    return fresh;
}

}  // namespace

int main(int argc, char** argv) {
    //   sprout_editor [project folder]
    //   sprout_editor [--project folder] --check-scripts
    //   sprout_editor [--project folder] --export linux,windows,web [output folder]
    std::string projectArg, exportTargets, exportOut;
    bool check = false, exporting = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--project" && i + 1 < argc) projectArg = argv[++i];
        else if (a == "--check-scripts") check = true;
        else if (a == "--export" && i + 1 < argc) {
            exporting = true;
            exportTargets = argv[++i];
            if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) exportOut = argv[++i];
        } else if (a.rfind("--", 0) != 0) projectArg = a;
    }
    // The command-line tools use the demo project unless told otherwise.
    if (check) return checkScripts(projectArg.empty() ? fs::path(SPROUT_EDITOR_PROJECT_DIR) : fs::path(projectArg));
    if (exporting)
        return exportGame(projectArg.empty() ? fs::path(SPROUT_EDITOR_PROJECT_DIR) : fs::path(projectArg), exportTargets,
                          exportOut);
    fs::path project = startupProject(projectArg);

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

    // One editor per project; picking another project in the Project Manager
    // closes this one and opens the next in the same window.
    long frame = 0;
    while (true) {
        s2d::projects::remember(project);
        std::optional<fs::path> next;
        {
            s2d::Editor editor(project);
            editor.setUiScale(scale);
            while (!window.ShouldClose()) {
                ++frame;
                if (screenshot && testFrames > 0 && frame == testFrames) editor.screenshotPath = screenshot;
                editor.frame();
                if (testFrames > 0 && frame >= testFrames) break;
                if ((next = editor.takeProjectSwitch())) break;
            }
            scale = editor.uiScale();
        }  // the editor frees its textures and scripts before the next one (or the window) goes
        if (!next) break;
        project = *next;
    }
    return 0;
}
