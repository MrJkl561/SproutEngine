// Sprout Editor: a small Godot-style 2D editor built on raylib, raylib-cpp and raygui.
//
//   sprout_editor [project folder]          open a project (default: the one open last time)
//   sprout_editor [--project folder] --check-scripts
//                                           build and load the project's C++ scripts without
//                                           opening a window, then exit (used by CI)
//   sprout_editor [--project folder] --export linux,web [output folder]
//                                           export the game without opening a window
//   sprout_editor [--project folder] --import-tiled map.tmx [scenes/map.scene.json]
//                                           turn a Tiled map into a scene (pictures outside the
//                                           project are copied into its assets/tiles/)
//   sprout_editor [--project folder] --play [--scene file] [--server] [--frames n] [-- game arguments]
//                                           run the game without the editor (what Run Instances
//                                           starts for each extra window; --server: no window)
//
// The command-line tools use the demo project (editor/project/) unless --project says otherwise.
#include <raylib-cpp.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "editor.hpp"
#include "export.hpp"
#include "editor_paths.hpp"
#include "game_runner.hpp"
#include "hot_reload.hpp"
#include "project.hpp"
#include "projects.hpp"
#include "raygui_include.hpp"
#include "script.hpp"
#include "script_checker.hpp"
#include "script_lint.hpp"
#include "assets.hpp"
#include "scene_io.hpp"
#include "theme.hpp"
#include "tiled_import.hpp"
#include "ui_scale.hpp"


namespace fs = std::filesystem;

namespace {

// Loads the project's scripts (building the hot reload library first, if this
// build uses one) and makes one of each, without a window. Proves the library
// builds, loads and links on this system.
int checkScripts(const fs::path& project) {
    s2d::ScriptReloader reloader(project / "scripts", s2d::projects::workDir(project, s2d::editorPaths().buildDir) / "scripts");
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

// Runs the project as a game, with its scripts (building them first if they
// changed and this is a hot reloading build).
int playGame(const fs::path& project, std::vector<std::string> gameArgs) {
    s2d::ScriptReloader reloader(project / "scripts", s2d::projects::workDir(project, s2d::editorPaths().buildDir) / "scripts");
    if (s2d::ScriptReloader::supported()) {
        std::string output, error;
        // (--no-build: the editor started us and builds the scripts itself)
        bool mayBuild = std::find(gameArgs.begin(), gameArgs.end(), "--no-build") == gameArgs.end();
        if (mayBuild && (!reloader.libraryExists() || reloader.libraryOutOfDate()) && !reloader.buildNow(output)) {
            std::printf("%s\nThe scripts didn't compile.\n", output.c_str());
            return 1;
        }
        if (!reloader.load(error)) {
            std::printf("Couldn't load the scripts: %s\n", error.c_str());
            return 1;
        }
    }
    s2d::GameOptions options = s2d::gameOptionsFromArgs(gameArgs);
    options.project = project;
    int code = s2d::runGame(options);
    reloader.unload();
    return code;
}

// Checks one script the way the Script tab does while you type, and prints what
// it finds. Exit code 0 if it has no errors.
int checkFile(const fs::path& project, const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        std::printf("Can't read %s\n", file.string().c_str());
        return 2;
    }
    std::stringstream text;
    text << in.rdbuf();
    const s2d::EditorPaths& paths = s2d::editorPaths();
    s2d::ScriptChecker checker({paths.clangd, paths.cxx, paths.cxxId, project / "scripts",
                                s2d::projects::workDir(project, s2d::editorPaths().buildDir), paths.srcDir, paths.depsDir});
    if (checker.engine().empty()) {
        std::printf("Nothing to check with (no clangd, and the compiler can't be used).\n");
        return 2;
    }
    checker.update(file, text.str(), 1);
    s2d::ScriptChecker::Result result;
    bool diagnostics = false, tokens = false;
    size_t tokenCount = 0;
    std::vector<s2d::Diagnostic> found;
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(60)) {
        checker.poll(std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
        if (checker.take(file, result)) {
            if (result.hasDiagnostics) {
                diagnostics = true;
                found = result.diagnostics;
            }
            if (result.hasTokens) {
                tokens = true;
                tokenCount = result.tokens.size();
            }
        }
        if (diagnostics && (tokens || checker.engine() != "clangd")) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (!diagnostics) {
        std::printf("No answer from %s.\n", checker.engine().c_str());
        return 2;
    }
    for (s2d::Diagnostic& d : s2d::lintScript(text.str())) found.push_back(std::move(d));  // (as the Script tab does)
    int errors = 0;
    for (const s2d::Diagnostic& d : found) {
        std::printf("%s:%d:%d: %s: %s\n", file.filename().string().c_str(), d.line, d.column,
                    d.error ? "error" : "warning", d.message.c_str());
        errors += d.error;
    }
    std::printf("Checked with %s: %d error(s), %d warning(s)", checker.engine().c_str(), errors,
                static_cast<int>(found.size()) - errors);
    if (tokens) std::printf(", %zu names colored", tokenCount);
    std::printf("\n");
    return errors ? 1 : 0;
}

// Exports the game from the command line, printing progress. Exit code 0 if
// every platform worked.
int exportGame(const fs::path& project, const std::string& targets, const std::string& outDir) {
    s2d::ProjectSettings settings = s2d::ProjectSettings::load(project);
    const s2d::EditorPaths& paths = s2d::editorPaths();
    s2d::Exporter exporter({paths.editorDir, project, project / "scripts",
                            s2d::projects::workDir(project, s2d::editorPaths().buildDir) / "exports",
                            paths.depsDir, paths.cmake.string(), paths.generator, paths.generatorInstance});
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

// Turns a Tiled map into a scene file. Exit code 0 if it worked.
int importTiled(const fs::path& project, const fs::path& map, std::string sceneArg) {
    s2d::Assets::setProjectDir(project);
    SetTraceLogLevel(LOG_WARNING);
    s2d::TiledImport result = s2d::importTiledMap(map, project);
    for (const std::string& w : result.warnings) std::printf("Warning: %s\n", w.c_str());
    if (!result.root) {
        std::printf("Couldn't import %s: %s\n", map.string().c_str(), result.error.c_str());
        return 1;
    }
    for (const std::string& c : result.copied) std::printf("Copied %s\n", c.c_str());
    if (sceneArg.empty()) sceneArg = "scenes/" + map.stem().string() + ".scene.json";
    std::string error;
    if (!s2d::saveScene(*result.root, project / sceneArg, &error)) {
        std::printf("Couldn't save %s: %s\n", sceneArg.c_str(), error.c_str());
        return 1;
    }
    std::printf("Imported %s as %s\n", map.filename().string().c_str(), sceneArg.c_str());
    return 0;
}

// The project to open: the one named on the command line, else the one open
// last time, else the demo project (else a new one in ~/SproutProjects).
fs::path startupProject(const std::string& fromCommandLine) {
    if (!fromCommandLine.empty()) return fromCommandLine;
    s2d::projects::Recent recent = s2d::projects::loadRecent();
    if (!recent.last.empty() && s2d::projects::isProject(recent.last)) return recent.last;
    if (s2d::projects::isProject(s2d::editorPaths().demoProject)) return s2d::editorPaths().demoProject;
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
    //   sprout_editor [--project folder] --check-file scripts/player.cpp   (live checking, as in the Script tab)
    //   sprout_editor [--project folder] --play [game options and arguments]
    std::string projectArg, exportTargets, exportOut, checkFileArg, tiledMap, tiledScene;
    bool check = false, exporting = false, playing = false;
    std::vector<std::string> gameArgs;  // for --play: its options, and everything after --
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (playing) {
            if (a != "--") gameArgs.push_back(a);
            continue;
        }
        if (a == "--project" && i + 1 < argc) projectArg = argv[++i];
        else if (a == "--play") playing = true;
        else if (a == "--check-scripts") check = true;
        else if (a == "--check-file" && i + 1 < argc) checkFileArg = argv[++i];
        else if (a == "--import-tiled" && i + 1 < argc) {
            tiledMap = argv[++i];
            if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) tiledScene = argv[++i];
        }
        else if (a == "--export" && i + 1 < argc) {
            exporting = true;
            exportTargets = argv[++i];
            if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) exportOut = argv[++i];
        } else if (a.rfind("--", 0) != 0) projectArg = a;
    }
    // The command-line tools use the demo project unless told otherwise.
    if (!checkFileArg.empty())
        return checkFile(projectArg.empty() ? s2d::editorPaths().demoProject : fs::path(projectArg), checkFileArg);
    if (!tiledMap.empty())
        return importTiled(projectArg.empty() ? s2d::editorPaths().demoProject : fs::path(projectArg), tiledMap, tiledScene);
    if (playing) return playGame(projectArg.empty() ? s2d::editorPaths().demoProject : fs::path(projectArg), gameArgs);
    if (check) return checkScripts(projectArg.empty() ? s2d::editorPaths().demoProject : fs::path(projectArg));
    if (exporting)
        return exportGame(projectArg.empty() ? s2d::editorPaths().demoProject : fs::path(projectArg), exportTargets,
                          exportOut);
    fs::path project = startupProject(projectArg);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    raylib::Window window(1280, 720, s2d::theme().title.c_str());
    // The theme's window icon, if it has one
    if (std::string icon = s2d::themePath(s2d::theme().images.windowIcon); !icon.empty() && FileExists(icon.c_str())) {
        Image image = LoadImage(icon.c_str());
        SetWindowIcon(image);
        UnloadImage(image);
    }

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
            while (!editor.quitting()) {
                if (window.ShouldClose()) editor.requestQuit();  // asks about unsaved changes first
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
