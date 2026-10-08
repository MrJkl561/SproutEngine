// Hot reloading of C++ scripts.
//
// A project's scripts (<project>/scripts/*.cpp) are built into a library
// (sprout_scripts) by editor/scripts_module, in a build folder of the
// project's own. ScriptReloader watches the scripts folder, rebuilds the
// library with CMake in the background when a file changes, and loads the new
// build: each REGISTER_SCRIPT in it adds the script to the ScriptRegistry again.
//
// Before unload() / load(), every Script object made from the old library must
// be destroyed (their code lives in it); the editor takes care of that.
#pragma once

#include <atomic>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace s2d {

class ScriptReloader {
public:
    // workDir: where this project's scripts library is built.
    ScriptReloader(std::filesystem::path scriptsDir, std::filesystem::path workDir);
    ~ScriptReloader();
    ScriptReloader(const ScriptReloader&) = delete;
    ScriptReloader& operator=(const ScriptReloader&) = delete;

    // False when the editor was built with the scripts compiled in (Windows).
    static bool supported();

    // Checks the scripts folder for changes and starts a build when needed.
    void update(double now);
    void requestBuild();  // rebuild as soon as possible
    bool buildNow(std::string& output);  // build and wait (for the command line)

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

    std::filesystem::path libraryPath() const;
    // Configure (the first time) + build. Static: a build can outlive its reloader.
    static bool runBuild(const std::filesystem::path& scriptsDir, const std::filesystem::path& workDir,
                         std::string& output);

    std::filesystem::path scriptsDir_;
    std::filesystem::path workDir_;
    std::map<std::string, std::filesystem::file_time_type> known_;
    [[maybe_unused]] double lastScan_ = -100;
    [[maybe_unused]] bool pending_ = false;
    [[maybe_unused]] double changedAt_ = 0;

    // A build in the background. It's shared with its thread, so closing the
    // project (or the editor) doesn't wait for a compile: the thread is let go and
    // finishes on its own.
    struct BuildJob {
        std::atomic<bool> done{false};
        bool ok = false;
        std::string output;
    };
    std::shared_ptr<BuildJob> job_;
    std::thread thread_;
    bool building_ = false;
    bool lastBuildFailed_ = false;

    void* handle_ = nullptr;
    std::filesystem::path loadedCopy_;
    [[maybe_unused]] int loadCount_ = 0;
    std::vector<std::string> names_;  // scripts registered by the loaded library
};

}  // namespace s2d
