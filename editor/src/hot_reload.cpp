#include "hot_reload.hpp"

#include <algorithm>
#include <cstdio>
#include <set>
#include <system_error>

#include "platform.hpp"
#include "script.hpp"

namespace fs = std::filesystem;

namespace s2d {

#if defined(SPROUT_EDITOR_HOT_RELOAD)

bool ScriptReloader::supported() { return true; }

ScriptReloader::ScriptReloader(fs::path scriptsDir) : scriptsDir_(std::move(scriptsDir)) { known_ = scanScripts(); }

ScriptReloader::~ScriptReloader() {
    if (thread_.joinable()) thread_.join();
    unload();
}

std::map<std::string, fs::file_time_type> ScriptReloader::scanScripts() const {
    std::map<std::string, fs::file_time_type> files;
    std::error_code ec;
    for (auto it = fs::directory_iterator(scriptsDir_, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
        auto ext = it->path().extension();
        if (!it->is_regular_file(ec) || (ext != ".cpp" && ext != ".hpp" && ext != ".h")) continue;
        files[it->path().string()] = it->last_write_time(ec);
    }
    return files;
}

void ScriptReloader::update(double now) {
    if (now - lastScan_ >= 0.5) {
        lastScan_ = now;
        auto files = scanScripts();
        if (files != known_) {
            known_ = std::move(files);
            pending_ = true;
            changedAt_ = now;
        }
    }
    // Wait a moment after the last change: editors often save in several steps.
    if (pending_ && !building_ && now - changedAt_ > 0.3) startBuild();
}

void ScriptReloader::requestBuild() {
    pending_ = true;
    changedAt_ = -100;
}

void ScriptReloader::startBuild() {
    pending_ = false;
    building_ = true;
    buildDone_ = false;
    if (thread_.joinable()) thread_.join();
    thread_ = std::thread([this] {
        std::string cmd = std::string("\"") + SPROUT_EDITOR_CMAKE + "\" --build \"" + SPROUT_EDITOR_BUILD_DIR +
                          "\" --target sprout_scripts --config \"" + SPROUT_EDITOR_BUILD_CONFIG + "\"";
        std::string output;
        bool ok = platform::runCommand(cmd, output) == 0;
        buildOk_ = ok;
        buildOutput_ = std::move(output);
        buildDone_ = true;  // published last; read by pollBuild() on the main thread
    });
}

bool ScriptReloader::pollBuild(bool& ok, std::string& output) {
    if (!building_ || !buildDone_) return false;
    thread_.join();
    building_ = false;
    ok = buildOk_;
    lastBuildFailed_ = !ok;
    output = std::move(buildOutput_);
    return true;
}

bool ScriptReloader::libraryExists() const {
    std::error_code ec;
    return fs::exists(SPROUT_EDITOR_SCRIPTS_MODULE, ec);
}

bool ScriptReloader::libraryOutOfDate() const {
    std::error_code ec;
    if (!libraryExists()) return true;
    auto built = fs::last_write_time(SPROUT_EDITOR_SCRIPTS_MODULE, ec);
    for (auto& [path, time] : known_)
        if (time > built) return true;
    return false;
}

bool ScriptReloader::load(std::string& error) {
    if (handle_) unload();
    std::error_code ec;
    // Load a copy with a new name each time: the system won't load the same
    // file name twice, and the build can replace the original while we run.
    fs::path copy = fs::temp_directory_path(ec) /
                    ("sprout_scripts_" + std::to_string(platform::processId()) + "_" + std::to_string(++loadCount_) +
                     fs::path(SPROUT_EDITOR_SCRIPTS_MODULE).extension().string());
    fs::copy_file(SPROUT_EDITOR_SCRIPTS_MODULE, copy, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        error = "couldn't copy the scripts library: " + ec.message();
        return false;
    }

    std::vector<std::string> before = ScriptRegistry::names();
    void* handle = platform::loadLibrary(copy, error);
    if (!handle) {
        fs::remove(copy, ec);
        return false;
    }
    handle_ = handle;
    loadedCopy_ = copy;

    // The library's REGISTER_SCRIPT lines ran while it loaded.
    std::set<std::string> old(before.begin(), before.end());
    names_.clear();
    for (auto& n : ScriptRegistry::names())
        if (!old.count(n)) names_.push_back(n);
    return true;
}

void ScriptReloader::unload() {
    if (!handle_) return;
    // The registry holds functions from the library; forget them first.
    for (auto& n : names_) ScriptRegistry::remove(n);
    names_.clear();
    platform::freeLibrary(handle_);
    handle_ = nullptr;
    std::error_code ec;
    fs::remove(loadedCopy_, ec);
}

#else  // scripts are compiled into the editor

bool ScriptReloader::supported() { return false; }
ScriptReloader::ScriptReloader(fs::path scriptsDir) : scriptsDir_(std::move(scriptsDir)) {}
ScriptReloader::~ScriptReloader() = default;
std::map<std::string, fs::file_time_type> ScriptReloader::scanScripts() const { return {}; }
void ScriptReloader::update(double) {}
void ScriptReloader::requestBuild() {}
void ScriptReloader::startBuild() {}
bool ScriptReloader::pollBuild(bool&, std::string&) { return false; }
bool ScriptReloader::libraryExists() const { return false; }
bool ScriptReloader::libraryOutOfDate() const { return false; }
bool ScriptReloader::load(std::string& error) {
    error = "hot reloading is turned off in this build";
    return false;
}
void ScriptReloader::unload() {}

#endif

}  // namespace s2d
