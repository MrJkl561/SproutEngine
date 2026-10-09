// The project's settings, kept in project/project.json: the game's name, the
// scene it starts with and its window size. Read by the editor and by
// exported games; the export settings are only used by the editor.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace s2d {

// A node made when the game starts and kept for as long as it runs, across
// scene changes (like Godot's autoloads): a script on a plain node, or a scene.
// Scripts reach it with autoload<GameState>("GameState").
struct Autoload {
    std::string name;    // what scripts look it up by, e.g. "GameState"
    std::string script;  // a script class (on a new Node)...
    std::string scene;   // ...or a scene file, e.g. "scenes/music.scene.json"
};

struct ProjectSettings {
    std::string name = "Sprout Game";
    std::string mainScene = "scenes/main.scene.json";
    int width = 800;  // the game window (and the purple frame in the editor)
    int height = 450;

    std::string exportDir;                 // where exports go ("" = editor/exports)
    std::vector<std::string> exportTargets;  // the platforms ticked in the Export window

    // Run Instances (like Godot's): Play starts this many copies of the game,
    // the first in the editor and the rest in windows of their own, each with
    // its own command line (commandLineArgs() in scripts), e.g. "--host" and
    // "--join 127.0.0.1". "--server" makes one a dedicated server (no window).
    static constexpr int kMaxInstances = 4;
    int runInstances = 1;
    std::vector<std::string> instanceArgs;  // one line per instance (may be shorter)

    std::vector<Autoload> autoloads;  // made in this order, before the main scene

    static ProjectSettings load(const std::filesystem::path& projectDir);  // defaults if there's no file
    bool save(const std::filesystem::path& projectDir) const;
};

}  // namespace s2d
