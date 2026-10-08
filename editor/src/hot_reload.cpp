#include "hot_reload.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <chrono>
#include <mutex>
#include <system_error>
#include <vector>

#include "platform.hpp"
#include "script.hpp"

namespace fs = std::filesystem;

namespace s2d {

#if defined(SPROUT_EDITOR_HOT_RELOAD)

bool ScriptReloader::supported() { return true; }

namespace {
// Builds still running, by build folder, including ones whose project was closed
// (two builds must never run in the same folder at once). Never destroyed, so
// a build that's still running when the editor quits can't touch a dead map.
struct RunningBuilds {
    std::mutex mutex;
    std::map<std::string, std::weak_ptr<void>> byFolder;
};
RunningBuilds& runningBuilds() {
    static auto* builds = new RunningBuilds;
    return *builds;
}
bool folderBusy(const fs::path& workDir) {
    RunningBuilds& b = runningBuilds();
    std::lock_guard<std::mutex> lock(b.mutex);
    auto it = b.byFolder.find(workDir.string());
    if (it == b.byFolder.end()) return false;
    if (it->second.expired()) {
        b.byFolder.erase(it);
        return false;
    }
    return true;
}
}  // namespace

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

bool ScriptReloader::runBuild(const fs::path& scriptsDir, const fs::path& workDir, std::string& output) {
    auto q = [](const fs::path& p) { return "\"" + p.generic_string() + "\""; };
    std::string cmake = q(SPROUT_EDITOR_CMAKE);
    std::error_code ec;
    if (!fs::exists(workDir / "CMakeCache.txt", ec)) {
        // First time for this project: set up its build folder.
        std::string common = cmake + " -S " + q(fs::path(SPROUT_EDITOR_DIR) / "scripts_module") + " -B " + q(workDir);
        std::string settings = std::string(" -DCMAKE_BUILD_TYPE=") + SPROUT_EDITOR_BUILD_CONFIG;
        settings += " -DSPROUT_SCRIPTS_DIR=" + q(scriptsDir);
        settings += " -DSPROUT_EDITOR_SRC=" + q(fs::path(SPROUT_EDITOR_DIR) / "src");
        settings += " -DSPROUT_DEPS_DIR=" + q(SPROUT_EDITOR_DEPS_DIR);
#if defined(SPROUT_EDITOR_IMPLIB)
        settings += " -DSPROUT_EDITOR_IMPLIB=" + q(SPROUT_EDITOR_IMPLIB);
#endif
        // The ways to try, best first: if one fails (say the Visual Studio that
        // built the editor has moved or been updated), the next one gets a go.
        std::vector<std::string> generators;
#if !defined(_WIN32)
        generators.push_back(platform::findProgram("ninja").empty() ? "" : " -G Ninja");
#elif defined(_MSC_VER)
        // The DLL has to be built for the editor's own CPU (x64 or ARM64), not
        // whatever Visual Studio would pick for this PC.
#if defined(_M_ARM64)
        const std::string arch = " -A ARM64";
#elif defined(_M_X64)
        const std::string arch = " -A x64";
#elif defined(_M_IX86)
        const std::string arch = " -A Win32";
#else
        const std::string arch;
#endif
        const std::string editorGenerator = SPROUT_EDITOR_GENERATOR;
        const std::string instance = SPROUT_EDITOR_GENERATOR_INSTANCE;
        const char* env = std::getenv("CMAKE_GENERATOR");
        bool envIsVs = !env || std::string(env).find("Visual Studio") != std::string::npos;
        if (editorGenerator.find("Visual Studio") != std::string::npos) {
            if (!instance.empty())
                generators.push_back(" -G \"" + editorGenerator + "\"" + arch + " \"-DCMAKE_GENERATOR_INSTANCE=" + instance + "\"");
            generators.push_back(" -G \"" + editorGenerator + "\"" + arch);
        }
        generators.push_back(envIsVs ? arch : "");  // whatever CMake picks
#else
        generators.push_back("");
#endif
        bool configured = false;
        std::string attempts;
        for (const std::string& generator : generators) {
            // A half-made build folder from a failed try would stop the next one
            fs::remove(workDir / "CMakeCache.txt", ec);
            fs::remove_all(workDir / "CMakeFiles", ec);
            std::string out;
            if (platform::runCommand(common + generator + settings, out) == 0) {
                configured = true;
                break;
            }
            attempts += out;
        }
        if (!configured) {
            output += attempts;
            fs::remove(workDir / "CMakeCache.txt", ec);  // try setting it up again next time
            return false;
        }
    }
    // The same build type as the editor (on Windows the two must match).
    std::string build = cmake + " --build " + q(workDir) + " --config " + SPROUT_EDITOR_BUILD_CONFIG;
    return platform::runCommand(build, output) == 0;
}

bool ScriptReloader::buildNow(std::string& output) {
    if (thread_.joinable()) thread_.join();
    while (folderBusy(workDir_)) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return runBuild(scriptsDir_, workDir_, output);
}

ScriptReloader::~ScriptReloader() {
    // Don't wait for a compile that's still going: let it finish by itself.
    if (thread_.joinable()) thread_.detach();
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
    // (and not while a build from before is still running in the same folder)
    if (pending_ && !building_ && now - changedAt_ > 0.3 && !folderBusy(workDir_)) startBuild();
}

void ScriptReloader::requestBuild() {
    pending_ = true;
    changedAt_ = -100;
}

void ScriptReloader::startBuild() {
    pending_ = false;
    building_ = true;
    if (thread_.joinable()) thread_.join();
    auto job = std::make_shared<BuildJob>();
    job_ = job;
    {
        RunningBuilds& b = runningBuilds();
        std::lock_guard<std::mutex> lock(b.mutex);
        b.byFolder[workDir_.string()] = job;
    }
    // The thread only uses its own copies, never `this` (which may be gone by the end).
    thread_ = std::thread([job, scriptsDir = scriptsDir_, workDir = workDir_] {
        std::string output;
        job->ok = runBuild(scriptsDir, workDir, output);
        job->output = std::move(output);
        job->done = true;  // published last; read by pollBuild() on the main thread
    });
}

bool ScriptReloader::pollBuild(bool& ok, std::string& output) {
    if (!building_ || !job_ || !job_->done) return false;
    thread_.join();
    building_ = false;
    ok = job_->ok;
    lastBuildFailed_ = !ok;
    output = std::move(job_->output);
    job_.reset();
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
bool ScriptReloader::runBuild(const fs::path&, const fs::path&, std::string&) { return true; }
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
