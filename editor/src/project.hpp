// The project's settings, kept in project/project.json: the game's name, the
// scene it starts with and its window size. Read by the editor and by
// exported games; the export settings are only used by the editor.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace s2d {

struct ProjectSettings {
    std::string name = "Sprout Game";
    std::string mainScene = "scenes/main.scene.json";
    int width = 800;  // the game window (and the purple frame in the editor)
    int height = 450;

    std::string exportDir;                 // where exports go ("" = editor/exports)
    std::vector<std::string> exportTargets;  // the platforms ticked in the Export window

    static ProjectSettings load(const std::filesystem::path& projectDir);  // defaults if there's no file
    bool save(const std::filesystem::path& projectDir) const;
};

}  // namespace s2d
