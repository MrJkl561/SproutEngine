// Runs a project as a game, without the editor: what exported games do, and
// what the editor starts for each extra window of Run Instances
// (sprout_editor --play). The scripts must already be loaded or compiled in.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace s2d {

struct GameOptions {
    std::filesystem::path project;  // the project folder (scenes, assets, project.json)
    std::string scene;              // "" = the project's main scene
    std::vector<std::string> args;  // the game's command line (commandLineArgs())
    bool headless = false;          // no window: a dedicated server (--server)
    long testFrames = 0;            // > 0: stop after this many frames (tests)
    std::string titleSuffix;        // added to the window title, e.g. " (2)"
    int windowX = -1, windowY = -1; // where the window goes (-1 = the system decides)
    bool stopWhenInputCloses = false;  // started by the editor: its Stop closes our input
};

// Returns the program's exit code.
int runGame(const GameOptions& options);

// Reads the options every game understands from a command line (without the
// program name): --server / --headless, --scene <file>, --frames <n>,
// --window-pos <x>,<y>, --instance <n> (started by the editor's Run Instances).
// Everything is also kept in options.args for scripts.
GameOptions gameOptionsFromArgs(const std::vector<std::string>& args);

}  // namespace s2d
