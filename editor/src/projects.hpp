// Projects: each game is a folder with
//
//   project.json   name, main scene, window size, export settings
//   scenes/        *.scene.json
//   assets/        pictures
//   scripts/       C++ scripts (built into a library the editor hot reloads)
//
// The editor remembers recently opened projects in a small settings file in
// your user folder (SPROUT_EDITOR_CONFIG_DIR changes where).
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace s2d::projects {

struct Recent {
    std::vector<std::filesystem::path> list;  // newest first
    std::filesystem::path last;               // opened the last time
};

Recent loadRecent();
void remember(const std::filesystem::path& project);  // puts it first in the list and makes it "last"
void forget(const std::filesystem::path& project);

bool isProject(const std::filesystem::path& dir);  // has a project.json (or a scenes/ folder)
std::string projectName(const std::filesystem::path& dir);  // from project.json, else the folder name

// Makes a new, empty project (with one scene) in dir, which must not exist or be empty.
bool createProject(const std::filesystem::path& dir, const std::string& name, std::string& error);

std::filesystem::path defaultProjectsFolder();  // ~/SproutProjects
// Where the editor builds things for a project (its scripts, its exports).
std::filesystem::path workDir(const std::filesystem::path& project, const std::filesystem::path& buildDir);

}  // namespace s2d::projects
