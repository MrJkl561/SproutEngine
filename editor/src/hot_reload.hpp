// Hot reloading of C++ scripts.
//
// The scripts in editor/scripts/ are built into a separate library
// (sprout_scripts). ScriptReloader watches that folder, rebuilds the library
// with CMake in the background when a file changes, and loads the new build:
// each REGISTER_SCRIPT in it adds the script to the ScriptRegistry again.
//
// Before unload() / load(), every Script object made from the old library must
// be destroyed (their code lives in it); the editor takes care of that.
#pragma once

#include <atomic>
#include <filesystem>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace s2d {

class ScriptReloader {
public:
    explicit ScriptReloader(std::filesystem::path scriptsDir);
    ~ScriptReloader();
    ScriptReloader(const ScriptReloader&) = delete;
    ScriptReloader& operator=(const ScriptReloader&) = delete;

    // False when the editor was built with the scripts compiled in (Windows).
    static bool supported();

    // Checks the scripts folder for changes and starts a build when needed.
    void update(double now);
    void requestBuild();  // rebuild as soon as possible

    // True once, when a build has finished. ok/output describe how it went.
    bool pollBuild(bool& ok, std::string& output);
    bool isBuilding() const { return building_; }
    bool lastBuildFailed() const { return lastBuildFailed_; }

    bool libraryExists() const;
    bool libraryOutOfDate() const;  // a script file is newer than the built library
    bool load(std::string& error);  // loads the built library (unload() first)
    void unload();
    bool isLoaded() const { return handle_ != nullptr; }
    const std::vector<std::string>& scriptNames() const { return names_; }

private:
    // ([[maybe_unused]]: these aren't used when hot reloading is turned off.)
    std::map<std::string, std::filesystem::file_time_type> scanScripts() const;
    void startBuild();

    std::filesystem::path scriptsDir_;
    std::map<std::string, std::filesystem::file_time_type> known_;
    [[maybe_unused]] double lastScan_ = -100;
    [[maybe_unused]] bool pending_ = false;
    [[maybe_unused]] double changedAt_ = 0;

    std::thread thread_;
    bool building_ = false;
    bool lastBuildFailed_ = false;
    std::atomic<bool> buildDone_{false};
    [[maybe_unused]] bool buildOk_ = false;
    std::string buildOutput_;

    void* handle_ = nullptr;
    std::filesystem::path loadedCopy_;
    [[maybe_unused]] int loadCount_ = 0;
    std::vector<std::string> names_;  // scripts registered by the loaded library
};

}  // namespace s2d
