#include "hot_reload.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <system_error>

#include "platform.hpp"
#include "script.hpp"

namespace fs = std::filesystem;

namespace s2d {

#if defined(SPROUT_EDITOR_HOT_RELOAD)

bool ScriptReloader::supported() { return true; }

ScriptReloader::ScriptReloader(fs::path scriptsDir, fs::path workDir)
    : scriptsDir_(std::move(scriptsDir)), workDir_(std::move(workDir)) {
    known_ = scanScripts();
}

fs::path ScriptReloader::libraryPath() const {
#if defined(_WIN32)
    return workDir_ / "out" / "sprout_scripts.dll";
#else
    return workDir_ / "out" / "sprout_scripts.so";
#endif
}

bool ScriptReloader::runBuild(std::string& output) const {
    auto q = [](const fs::path& p) { return "\"" + p.generic_string() + "\""; };
    std::string cmake = q(SPROUT_EDITOR_CMAKE);
    std::error_code ec;
    if (!fs::exists(workDir_ / "CMakeCache.txt", ec)) {
        std::string cmd;
        // First time for this project: set up its build folder.
        cmd = cmake + " -S " + q(fs::path(SPROUT_EDITOR_DIR) / "scripts_module") + " -B " + q(workDir_);
#if !defined(_WIN32)
        if (!platform::findProgram("ninja").empty()) cmd += " -G Ninja";
#elif defined(_MSC_VER)
        // The DLL has to be built for the editor's own CPU (x64 or ARM64), not
        // whatever Visual Studio would pick for this PC.
        const std::string editorGenerator = SPROUT_EDITOR_GENERATOR;
        const std::string instance = SPROUT_EDITOR_GENERATOR_INSTANCE;
        if (editorGenerator.find("Visual Studio") != std::string::npos) {
            cmd += " -G \"" + editorGenerator + "\"";
            if (!instance.empty()) cmd += " \"-DCMAKE_GENERATOR_INSTANCE=" + instance + "\"";
        }
        const char* generator = std::getenv("CMAKE_GENERATOR");
        if (editorGenerator.find("Visual Studio") != std::string::npos ||
            !generator || std::string(generator).find("Visual Studio") != std::string::npos) {
#if defined(_M_ARM64)
            cmd += " -A ARM64";
#elif defined(_M_X64)
            cmd += " -A x64";
#elif defined(_M_IX86)
            cmd += " -A Win32";
#endif
        }
#endif
        cmd += std::string(" -DCMAKE_BUILD_TYPE=") + SPROUT_EDITOR_BUILD_CONFIG;
        cmd += " -DSPROUT_SCRIPTS_DIR=" + q(scriptsDir_);
        cmd += " -DSPROUT_EDITOR_SRC=" + q(fs::path(SPROUT_EDITOR_DIR) / "src");
        cmd += " -DSPROUT_DEPS_DIR=" + q(SPROUT_EDITOR_DEPS_DIR);
#if defined(SPROUT_EDITOR_IMPLIB)
        cmd += " -DSPROUT_EDITOR_IMPLIB=" + q(SPROUT_EDITOR_IMPLIB);
#endif
        if (platform::runCommand(cmd, output) != 0) {
            fs::remove(workDir_ / "CMakeCache.txt", ec);  // try setting it up again next time
            return false;
        }
    }
    // The same build type as the editor (on Windows the two must match).
    std::string build = cmake + " --build " + q(workDir_) + " --config " + SPROUT_EDITOR_BUILD_CONFIG;
    return platform::runCommand(build, output) == 0;
}

bool ScriptReloader::buildNow(std::string& output) {
    if (thread_.joinable()) thread_.join();
    return runBuild(output);
}

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
        std::string output;
        bool ok = runBuild(output);
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
    return fs::exists(libraryPath(), ec);
}

bool ScriptReloader::libraryOutOfDate() const {
    std::error_code ec;
    if (!libraryExists()) return true;
    auto built = fs::last_write_time(libraryPath(), ec);
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
                     libraryPath().extension().string());
    fs::copy_file(libraryPath(), copy, fs::copy_options::overwrite_existing, ec);
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
ScriptReloader::ScriptReloader(fs::path scriptsDir, fs::path workDir)
    : scriptsDir_(std::move(scriptsDir)), workDir_(std::move(workDir)) {}
fs::path ScriptReloader::libraryPath() const { return {}; }
bool ScriptReloader::runBuild(std::string&) const { return true; }
bool ScriptReloader::buildNow(std::string&) { return true; }
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
