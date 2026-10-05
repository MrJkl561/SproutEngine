#include "export.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "platform.hpp"
#include "projects.hpp"

namespace fs = std::filesystem;

namespace s2d {

namespace {

#if defined(_WIN32)
const char* kHost = "windows";
#elif defined(__APPLE__)
const char* kHost = "macos";
#else
const char* kHost = "linux";
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
const char* kHostArch = "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
const char* kHostArch = "x86_64";
#else
const char* kHostArch = "this kind of";
#endif

std::string quote(const fs::path& p) { return "\"" + p.generic_string() + "\""; }

bool isWindowsTarget(const std::string& id) { return id.rfind("windows", 0) == 0; }

// Visual Studio's name for the CPU a Windows export is built for.
const char* vsPlatform(const std::string& id) { return id == "windows-arm64" ? "ARM64" : "x64"; }

// True if CMake will use Visual Studio here (its default on Windows, unless
// CMAKE_GENERATOR says otherwise).
bool usesVisualStudio() {
    const char* g = std::getenv("CMAKE_GENERATOR");
    return !g || std::string(g).find("Visual Studio") != std::string::npos;
}

std::string quoteNative(fs::path p);

// Whether Visual Studio's ARM64 C++ tools are installed (an optional part of its
// installer; without them it can't build for ARM64).
bool vsHasArm64Tools() {
#if defined(_WIN32)
    const char* x86 = std::getenv("ProgramFiles(x86)");
    if (!x86) return false;
    fs::path vswhere = fs::path(x86) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe";
    std::error_code ec;
    if (!fs::exists(vswhere, ec)) return false;
    std::string out;
    platform::runCommand(quoteNative(vswhere) +
                             " -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.ARM64"
                             " -property installationPath",
                         out);
    return out.find_first_not_of(" \r\n") != std::string::npos;
#else
    return false;
#endif
}

// A build folder set up for another CPU (CMake refuses to change it): start it again.
void resetIfOtherPlatform(const fs::path& build, const std::string& platformName) {
    std::ifstream cache(build / "CMakeCache.txt");
    if (!cache) return;
    std::string line, used;
    while (std::getline(cache, line))
        if (line.rfind("CMAKE_GENERATOR_PLATFORM:", 0) == 0) used = line.substr(line.find('=') + 1);
    cache.close();
    if (used != platformName) {
        std::error_code ec;
        fs::remove_all(build, ec);
    }
}

#if defined(_WIN32)
// Emscripten has no Windows ARM64 build, so emsdk is always asked for the x64 one
// (Windows on ARM runs it through its x64 emulation; on x64 PCs it changes nothing).
const char* kEmsdkArch = "set \"EMSDK_ARCH=x86_64\" && ";
#endif

// A path for the command line: native slashes on Windows (cmd.exe's `call` needs them).
std::string quoteNative(fs::path p) { return "\"" + p.make_preferred().string() + "\""; }

fs::path emcmakeIn(const fs::path& emsdk) {
#if defined(_WIN32)
    const char* names[] = {"emcmake.exe", "emcmake.bat"};  // newer Emscripten has .exe launchers
#else
    const char* names[] = {"emcmake"};
#endif
    for (const char* name : names) {
        fs::path p = emsdk / "upstream" / "emscripten" / name;
        std::error_code ec;
        if (fs::is_regular_file(p, ec)) return p;
    }
    return {};
}

// emcmake, and what to run first so Emscripten's tools work (its python, node...).
// From a terminal where emsdk_env already ran, emcmake is simply on the PATH.
struct WebTools {
    fs::path emcmake;
    std::string env;  // a command prefix, e.g. call "...\emsdk_env.bat" >nul &&
};
WebTools findWebTools() {
    WebTools t;
    if (fs::path p = platform::findProgram("emcmake"); !p.empty()) {
        t.emcmake = p;
        return t;
    }
    fs::path emsdk = findEmsdk();
    if (emsdk.empty() || !emsdkInstalled(emsdk)) return t;
    t.emcmake = emcmakeIn(emsdk);
#if defined(_WIN32)
    t.env = std::string(kEmsdkArch) + "call " + quoteNative(emsdk / "emsdk_env.bat") + " >nul 2>&1 && ";
#endif
    return t;
}

// Ninja: on the PATH, or (Windows) the one that comes with Visual Studio.
fs::path findNinja() {
    if (fs::path p = platform::findProgram("ninja"); !p.empty()) return p;
#if defined(_WIN32)
    std::error_code ec;
    std::vector<fs::path> roots;
    if (const char* x86 = std::getenv("ProgramFiles(x86)")) {
        fs::path vswhere = fs::path(x86) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe";
        if (fs::exists(vswhere, ec)) {
            std::string out;
            platform::runCommand(quoteNative(vswhere) + " -all -property installationPath", out);
            std::stringstream lines(out);
            for (std::string line; std::getline(lines, line);) {
                while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
                if (!line.empty()) roots.push_back(line);
            }
        }
    }
    // winget install Ninja-build.Ninja (found even before the editor is restarted)
    if (const char* local = std::getenv("LOCALAPPDATA")) {
        fs::path links = fs::path(local) / "Microsoft" / "WinGet" / "Links" / "ninja.exe";
        if (fs::exists(links, ec)) return links;
        fs::path packages = fs::path(local) / "Microsoft" / "WinGet" / "Packages";
        for (fs::directory_iterator it(packages, ec), end; !ec && it != end; it.increment(ec))
            if (it->path().filename().string().rfind("Ninja-build.Ninja", 0) == 0 && fs::exists(it->path() / "ninja.exe", ec))
                return it->path() / "ninja.exe";
    }
    if (const char* pf = std::getenv("ProgramFiles"))
        for (const char* year : {"2022", "2019", "18"})
            for (const char* edition : {"Community", "Professional", "Enterprise", "BuildTools"})
                roots.push_back(fs::path(pf) / "Microsoft Visual Studio" / year / edition);
    for (const fs::path& root : roots) {
        fs::path p = root / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" / "Ninja" / "ninja.exe";
        if (fs::exists(p, ec)) return p;
    }
#endif
    return {};
}

fs::path findMingw() { return platform::findProgram("x86_64-w64-mingw32-g++"); }

void copyTree(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    fs::create_directories(to, ec);
    fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec) throw fs::filesystem_error("copying " + from.string(), from, to, ec);
}

// The project's files that go into a game: everything except exports, the
// scripts' source code (they're compiled in) and hidden folders like .git.
void stageProject(const fs::path& project, const fs::path& stage, const fs::path& outDir) {
    std::error_code ec;
    fs::remove_all(stage, ec);
    fs::create_directories(stage, ec);
    fs::path outAbs = fs::weakly_canonical(fs::absolute(outDir, ec), ec);
    for (auto& entry : fs::directory_iterator(project, ec)) {
        std::string name = entry.path().filename().string();
        if (name == "exports" || name == "scripts" || name.empty() || name[0] == '.') continue;
        fs::path abs = fs::weakly_canonical(fs::absolute(entry.path(), ec), ec);
        if (abs == outAbs) continue;  // a custom export folder inside the project
        if (entry.is_directory(ec)) copyTree(entry.path(), stage / name);
        else fs::copy_file(entry.path(), stage / name, fs::copy_options::overwrite_existing);
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// Emscripten (emsdk)
// ---------------------------------------------------------------------------
namespace {
fs::path toolsFile() { return projects::configDir() / "tools.json"; }
}  // namespace

bool isEmsdkFolder(const fs::path& folder) {
    std::error_code ec;
    return !folder.empty() && fs::is_regular_file(folder / "emsdk.py", ec);
}

bool emsdkInstalled(const fs::path& emsdk) {
    std::error_code ec;
    // `emsdk activate` writes .emscripten (the tools' settings) next to emsdk.py
    return !emcmakeIn(emsdk).empty() && fs::exists(emsdk / ".emscripten", ec);
}

void setEmsdkFolder(const fs::path& folder) {
    nlohmann::json j = nlohmann::json::object();
    {
        std::ifstream in(toolsFile());
        if (in) j = nlohmann::json::parse(in, nullptr, false);
        if (!j.is_object()) j = nlohmann::json::object();
    }
    j["emsdk"] = folder.string();
    std::error_code ec;
    fs::create_directories(projects::configDir(), ec);
    std::ofstream(toolsFile()) << j.dump(2) << '\n';
}

fs::path findEmsdk() {
    std::vector<fs::path> candidates;
    {
        std::ifstream in(toolsFile());
        nlohmann::json j = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
        if (j.is_object() && j.contains("emsdk") && j["emsdk"].is_string())
            candidates.push_back(j["emsdk"].get<std::string>());
    }
    if (const char* env = std::getenv("EMSDK")) candidates.push_back(env);
    std::vector<fs::path> homes;
    if (const char* h = std::getenv("USERPROFILE")) homes.push_back(h);
    if (const char* h = std::getenv("HOME")) homes.push_back(h);
    for (const fs::path& home : homes)
        for (const char* sub : {"emsdk", "Desktop/emsdk", "OneDrive/Desktop/emsdk", "Documents/emsdk",
                                "OneDrive/Documents/emsdk", "source/repos/emsdk", "dev/emsdk", "tools/emsdk"})
            candidates.push_back(home / sub);
#if defined(_WIN32)
    candidates.push_back("C:/emsdk");
    candidates.push_back("C:/tools/emsdk");
#else
    candidates.push_back("/opt/emsdk");
    candidates.push_back("/usr/local/emsdk");
#endif
    // Prefer one that's ready to use
    for (const fs::path& c : candidates)
        if (isEmsdkFolder(c) && emsdkInstalled(c)) return c;
    for (const fs::path& c : candidates)
        if (isEmsdkFolder(c)) return c;
    return {};
}

bool Exporter::setUpEmsdk(const fs::path& emsdk) {
    if (busy_ || !isEmsdkFolder(emsdk)) return false;
    wait();
    busy_ = true;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_ = "Emscripten setup";
    }
    thread_ = std::thread([this, emsdk] {
        say("== Setting up Emscripten in " + emsdk.string() + " ==", 3);
        say("This downloads about 1 GB the first time, so it can take a while.");
#if defined(_WIN32)
        // emsdk's own Python (once a first install has fetched it) runs emsdk.py
        // directly: no emsdk.bat, no nested cmd.exe.
        std::string tool = quoteNative(emsdk / "emsdk.bat");
        std::error_code ec;
        for (fs::directory_iterator it(emsdk / "python", ec), end; !ec && it != end; it.increment(ec))
            if (fs::exists(it->path() / "python.exe", ec)) {
                tool = "set \"PYTHONHOME=\" && set \"PYTHONPATH=\" && " + quoteNative(it->path() / "python.exe") + " " +
                       quoteNative(emsdk / "emsdk.py");
                break;
            }
        std::string cd = std::string(kEmsdkArch) + "cd /d " + quoteNative(emsdk) + " && ";
        const char* noInput = " <nul";
#else
        std::string tool = "./emsdk";
        std::string cd = "cd " + quote(emsdk) + " && ";
        const char* noInput = " </dev/null";
#endif
        bool ok = true;
        // install is quick when everything's there ("already installed"), and makes sure
        // nothing is missing (activate fails if, say, node wasn't downloaded)
        std::vector<std::string> steps{"install latest", "activate latest"};
        for (const std::string& step : steps) {
            say("emsdk " + step + "...");
            std::string output;
            int code = platform::runCommand(cd + tool + " " + step + noInput, output, [&](const std::string& line) {
                bool bad = line.find("error") != std::string::npos || line.find("Error") != std::string::npos;
                say("  " + line, bad ? 2 : 0);
            });
            if (code != 0) {
                say("emsdk " + step + " failed (exit code " + std::to_string(code) + ").", 2);
#if defined(_WIN32)
                say("If it says Python wasn't found: install Python from python.org (tick \"Add python.exe to "
                    "PATH\"), restart the editor and press Set Up again.", 2);
#endif
                ok = false;
                break;
            }
        }
        if (ok) {
            setEmsdkFolder(emsdk);
            say("Emscripten is ready: tick Web in the Export window.", 1);
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            current_.clear();
        }
        busy_ = false;
    });
    return true;
}

std::string programName(const std::string& gameName) {
    std::string out;
    for (char c : gameName) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
        out += ok ? c : '_';
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    return out.empty() ? "game" : out;
}

Exporter::Exporter(Paths paths) : paths_(std::move(paths)) {}

Exporter::~Exporter() { wait(); }

void Exporter::wait() {
    if (thread_.joinable()) thread_.join();
}

std::vector<ExportTarget> Exporter::detectTargets() const {
    std::string host = kHost;
    std::vector<ExportTarget> list;

    ExportTarget onLinux{"linux", "Linux", host == "linux", ""};
    onLinux.note = onLinux.available
                       ? std::string("Built with this computer's compiler, for ") + kHostArch + " Linux PCs."
                       : "Needs Linux (on Windows, run the editor in WSL).";
    list.push_back(onLinux);

    ExportTarget windows{"windows", "Windows", false, ""};
    if (host == "windows") {
        windows.available = true;
        windows.note = "x64 .exe: runs on every Windows PC (ARM64 ones too, through emulation).";
    } else if (!findMingw().empty()) {
        windows.available = true;
        windows.note = "Built with MinGW-w64 (64-bit .exe).";
    } else {
        windows.note = host == "macos" ? "Install MinGW-w64: brew install mingw-w64"
                                       : "Install MinGW-w64: sudo apt install mingw-w64";
    }
    list.push_back(windows);
    if (host == "windows") {
        // A native ARM64 build as well (faster on ARM64 PCs; doesn't run on x64 ones)
        ExportTarget arm{"windows-arm64", "Windows ARM64", false, ""};
        if (!usesVisualStudio()) {
            arm.note = "Needs Visual Studio (CMAKE_GENERATOR is set to something else).";
        } else if (!vsHasArm64Tools()) {
            arm.note = "Add \"MSVC ARM64/ARM64EC build tools\" in the Visual Studio Installer (Modify > Individual components).";
        } else {
            arm.available = true;
            arm.note = "Native ARM64 .exe for Windows on ARM (doesn't run on x64 PCs).";
        }
        list.push_back(arm);
    }

    ExportTarget mac{"macos", "macOS", host == "macos", ""};
    mac.note = mac.available ? "A universal .app: runs on Apple Silicon and Intel Macs."
                             : "Needs a Mac: Apple's compiler and SDK only run on macOS.";
    list.push_back(mac);

    ExportTarget web{"web", "Web", false, ""};
    WebTools tools = findWebTools();
    fs::path emsdk = findEmsdk();
    if (tools.emcmake.empty() && emsdk.empty()) {
        web.note = "Needs Emscripten: get emsdk (emscripten.org), then pick its folder below.";
    } else if (tools.emcmake.empty()) {
        std::error_code ec;
        web.note = emcmakeIn(emsdk).empty()
                       ? "emsdk found, but Emscripten isn't installed in it yet: press Set Up below."
                   : !fs::exists(emsdk / ".emscripten", ec)
                       ? "Emscripten is installed but not activated (no .emscripten file): press Set Up below."
                       : "emsdk found but not ready: press Set Up below.";
    } else if (host == "windows" && findNinja().empty()) {
        web.note = "Also needs Ninja: winget install Ninja-build.Ninja (or Visual Studio's C++ CMake tools).";
    } else {
        web.available = true;
        web.note = "Built with Emscripten (a web page).";
    }
    list.push_back(web);
    return list;
}

void Exporter::say(const std::string& text, int kind) {
    std::lock_guard<std::mutex> lock(mutex_);
    lines_.push_back({text, kind});
}

std::vector<Exporter::Line> Exporter::takeLines() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Line> out;
    out.swap(lines_);
    return out;
}

std::string Exporter::currentLabel() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;
}

bool Exporter::start(const std::vector<std::string>& targetIds, const std::string& gameName, const fs::path& outDir) {
    if (busy_) return false;
    wait();
    std::vector<ExportTarget> chosen;
    for (const ExportTarget& t : detectTargets()) {
        for (const std::string& id : targetIds) {
            if (t.id != id) continue;
            if (t.available) chosen.push_back(t);
            else say("Skipping " + t.label + ": " + t.note, 2);
        }
    }
    if (chosen.empty()) {
        say("Nothing to export: pick at least one platform that's ready.", 2);
        return false;
    }
    busy_ = true;
    std::string program = programName(gameName);
    thread_ = std::thread([this, chosen, program, gameName, outDir] {
        int ok = 0;
        std::vector<std::string> failed;
        for (const ExportTarget& t : chosen) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                current_ = t.label;
            }
            bool good = false;
            try {
                good = exportOne(t, program, gameName, outDir);
            } catch (const std::exception& e) {
                say(std::string("Export failed: ") + e.what(), 2);
            }
            if (good) ++ok;
            else failed.push_back(t.label);
        }
        if (failed.empty()) {
            say("Export finished: " + std::to_string(ok) + " platform(s) in " + outDir.string(), 1);
        } else {
            std::string names;
            for (auto& f : failed) names += (names.empty() ? "" : ", ") + f;
            say("Export finished with problems. Failed: " + names + " (see the messages above).", 2);
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            current_.clear();
        }
        busy_ = false;
    });
    return true;
}

bool Exporter::exportOne(const ExportTarget& t, const std::string& program, const std::string& gameName,
                         const fs::path& outDir) {
    say("== Exporting " + t.label + " ==", 3);
    fs::path build = paths_.workDir / t.id;
    fs::path stage = build / "stage" / "project";
    fs::path deps = paths_.depsDir;
    std::string cmake = quote(paths_.cmake);
    std::string host = kHost;
    fs::path ninjaPath = findNinja();
    bool ninja = !ninjaPath.empty();
    WebTools web = t.id == "web" ? findWebTools() : WebTools{};

    // 1. Configure (only slow the first time: it builds raylib for that platform)
    std::string configure;
    if (t.id == "web") configure = web.env + quote(web.emcmake) + " ";
    configure += cmake + " -S " + quote(paths_.editorDir / "player") + " -B " + quote(build);
    bool native = t.id == host || (host == "windows" && isWindowsTarget(t.id));
    bool visualStudio = native && host == "windows" && usesVisualStudio();
    if (visualStudio) {
        // Visual Studio would build for this PC's own CPU: say which one is wanted.
        resetIfOtherPlatform(build, vsPlatform(t.id));
        configure += std::string(" -A ") + vsPlatform(t.id);
    } else if (ninja && !(native && host == "windows")) {
        configure += " -G Ninja";
        if (platform::findProgram("ninja").empty()) configure += " -DCMAKE_MAKE_PROGRAM=" + quote(ninjaPath);
    }
    configure += " -DCMAKE_BUILD_TYPE=Release";
    configure += " -DSPROUT_GAME_NAME=" + program;
    configure += " -DSPROUT_GAME_PROJECT_DIR=" + quote(stage);
    configure += " -DSPROUT_GAME_SCRIPTS_DIR=" + quote(paths_.scriptsDir);
    configure += " -DFETCHCONTENT_SOURCE_DIR_RAYLIB=" + quote(deps / "raylib-src");
    configure += " -DFETCHCONTENT_SOURCE_DIR_RAYLIB_CPP=" + quote(deps / "raylib_cpp-src");
    configure += " -DFETCHCONTENT_SOURCE_DIR_RAYGUI=" + quote(deps / "raygui-src");
    configure += " -DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON=" + quote(deps / "nlohmann_json-src");
    if (t.id == "windows" && host != "windows")
        configure += " -DCMAKE_TOOLCHAIN_FILE=" + quote(paths_.editorDir / "player" / "mingw-w64.cmake");
    if (t.id == "web") configure += " -DPLATFORM=Web";
    // One app for both kinds of Mac
    if (t.id == "macos") configure += " \"-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64\"";

    auto run = [&](const std::string& what, const std::string& cmd) {
        say(what + "...");
        std::string output;
        int code = platform::runCommand(cmd, output, [&](const std::string& line) {
            bool bad = line.find("error") != std::string::npos || line.find("Error") != std::string::npos;
            say("  " + line, bad ? 2 : 0);
        });
        if (code != 0) say(what + " failed (exit code " + std::to_string(code) + ").", 2);
        return code == 0;
    };
    stageProject(paths_.projectDir, stage, outDir);  // (after a reset above)
    if (!run("Configuring " + t.label, configure)) return false;

    // 2. Build (the old output is removed first, so the game is always re-linked
    // with the current project files). On macOS only the program inside the .app
    // goes: CMake writes the app's Info.plist when configuring, not when building.
    std::error_code ec;
    if (t.id == "macos") fs::remove(build / "out" / (program + ".app") / "Contents" / "MacOS" / program, ec);
    else fs::remove_all(build / "out", ec);
    if (!run("Building " + t.label, web.env + cmake + " --build " + quote(build) + " --config Release --parallel"))
        return false;

    // 3. Package: the program and the project folder, side by side
    fs::path out = build / "out";
    fs::path dest = outDir / t.id;
    fs::remove_all(dest, ec);
    fs::create_directories(dest, ec);
    if (t.id == "web") {
        for (const char* ext : {".js", ".wasm", ".data"}) fs::copy_file(out / (program + ext), dest / (program + ext));
        // The page, titled with the game's name
        std::ifstream page(out / (program + ".html"));
        std::stringstream html;
        html << page.rdbuf();
        std::string text = html.str();
        if (size_t at = text.find("<title>Game</title>"); at != std::string::npos)
            text.replace(at, 18, "<title>" + gameName + "</title>");
        std::ofstream(dest / "index.html") << text;
        std::ofstream howTo(dest / "HOW-TO-PLAY.txt");
        howTo << "This is a web build. Browsers won't run it straight from the disk, so serve the\n"
                 "folder with any web server and open index.html, for example:\n\n"
                 "    python -m http.server 8000      then visit http://localhost:8000\n\n"
                 "or upload the folder to a site such as itch.io (as an HTML game).\n";
    } else if (t.id == "macos") {
        fs::path app = dest / (program + ".app");
        copyTree(out / (program + ".app"), app);
        copyTree(paths_.projectDir, app / "Contents" / "Resources" / "project");
        // Sign it again ("ad-hoc", no Apple account needed) now that the project is
        // inside, or a downloaded copy is reported as damaged instead of opening.
        if (!run("Signing " + t.label, "codesign --force --deep --sign - " + quote(app))) return false;
    } else {
        std::string exe = program + (isWindowsTarget(t.id) ? ".exe" : "");
        fs::copy_file(out / exe, dest / exe);
        copyTree(stage, dest / "project");
    }
    say("Exported " + t.label + " to " + dest.string(), 1);
    return true;
}

}  // namespace s2d
