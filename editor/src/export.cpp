#include "export.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "platform.hpp"

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

std::string quote(const fs::path& p) { return "\"" + p.generic_string() + "\""; }

fs::path findEmcmake() {
    if (fs::path p = platform::findProgram("emcmake"); !p.empty()) return p;
    if (const char* emsdk = std::getenv("EMSDK")) {
        for (const char* name : {"emcmake", "emcmake.bat"}) {
            fs::path p = fs::path(emsdk) / "upstream" / "emscripten" / name;
            std::error_code ec;
            if (fs::is_regular_file(p, ec)) return p;
        }
    }
    return {};
}

fs::path findMingw() { return platform::findProgram("x86_64-w64-mingw32-g++"); }

void copyTree(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    fs::create_directories(to, ec);
    fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec) throw fs::filesystem_error("copying " + from.string(), from, to, ec);
}

}  // namespace

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
    onLinux.note = onLinux.available ? "Built with this computer's compiler."
                                 : "Needs Linux (on Windows, run the editor in WSL).";
    list.push_back(onLinux);

    ExportTarget windows{"windows", "Windows", false, ""};
    if (host == "windows") {
        windows.available = true;
        windows.note = "Built with this computer's compiler.";
    } else if (!findMingw().empty()) {
        windows.available = true;
        windows.note = "Built with MinGW-w64 (64-bit .exe).";
    } else {
        windows.note = host == "macos" ? "Install MinGW-w64: brew install mingw-w64"
                                       : "Install MinGW-w64: sudo apt install mingw-w64";
    }
    list.push_back(windows);

    ExportTarget mac{"macos", "macOS", host == "macos", ""};
    mac.note = mac.available ? "Built with this Mac's compiler (.app)."
                             : "Needs a Mac: Apple's compiler and SDK only run on macOS.";
    list.push_back(mac);

    ExportTarget web{"web", "Web", false, ""};
    fs::path emcmake = findEmcmake();
    if (emcmake.empty()) {
        web.note = "Install Emscripten (emsdk), and start the editor from a terminal where emsdk_env has run.";
    } else if (host == "windows" && platform::findProgram("ninja").empty()) {
        web.note = "Emscripten found, but web builds on Windows also need Ninja on the PATH.";
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
    fs::path deps = paths_.depsDir;
    std::string cmake = quote(paths_.cmake);
    std::string host = kHost;
    bool ninja = !platform::findProgram("ninja").empty();

    // 1. Configure (only slow the first time: it builds raylib for that platform)
    std::string configure;
    if (t.id == "web") configure = quote(findEmcmake()) + " ";
    configure += cmake + " -S " + quote(paths_.editorDir / "player") + " -B " + quote(build);
    bool native = t.id == host;
    if (ninja && !(native && host == "windows")) configure += " -G Ninja";  // VS on Windows picks its own
    configure += " -DCMAKE_BUILD_TYPE=Release";
    configure += " -DSPROUT_GAME_NAME=" + program;
    configure += " -DSPROUT_GAME_PROJECT_DIR=" + quote(paths_.projectDir);
    configure += " -DSPROUT_GAME_SCRIPTS_DIR=" + quote(paths_.scriptsDir);
    configure += " -DFETCHCONTENT_SOURCE_DIR_RAYLIB=" + quote(deps / "raylib-src");
    configure += " -DFETCHCONTENT_SOURCE_DIR_RAYLIB_CPP=" + quote(deps / "raylib_cpp-src");
    configure += " -DFETCHCONTENT_SOURCE_DIR_RAYGUI=" + quote(deps / "raygui-src");
    configure += " -DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON=" + quote(deps / "nlohmann_json-src");
    if (t.id == "windows" && host != "windows")
        configure += " -DCMAKE_TOOLCHAIN_FILE=" + quote(paths_.editorDir / "player" / "mingw-w64.cmake");
    if (t.id == "web") configure += " -DPLATFORM=Web";

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
    if (!run("Configuring " + t.label, configure)) return false;

    // 2. Build (the old output is removed first, so the game is always re-linked
    // with the current project files). On macOS only the program inside the .app
    // goes: CMake writes the app's Info.plist when configuring, not when building.
    std::error_code ec;
    if (t.id == "macos") fs::remove(build / "out" / (program + ".app") / "Contents" / "MacOS" / program, ec);
    else fs::remove_all(build / "out", ec);
    if (!run("Building " + t.label, cmake + " --build " + quote(build) + " --config Release --parallel")) return false;

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
    } else {
        std::string exe = program + (t.id == "windows" ? ".exe" : "");
        fs::copy_file(out / exe, dest / exe);
        copyTree(paths_.projectDir, dest / "project");
    }
    say("Exported " + t.label + " to " + dest.string(), 1);
    return true;
}

}  // namespace s2d
