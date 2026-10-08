// Exporting the game: builds the game runtime (editor/player) with your
// scripts for a platform, then packages it with the project folder.
//
//   linux    a Linux program + project/        (needs Linux)
//   windows  a Windows .exe + project/         (Windows, or MinGW-w64 on Linux/macOS)
//   macos    a .app                            (needs a Mac)
//   web      index.html + .js/.wasm/.data      (needs Emscripten)
//
// It runs CMake in the background; progress arrives as lines of text.
#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace s2d {

struct ExportTarget {
    std::string id;     // "linux", "windows", "macos", "web"
    std::string label;  // "Linux", ...
    bool available = false;
    std::string note;  // how it'll be built, or what's missing
};

class Exporter {
public:
    struct Paths {
        std::filesystem::path editorDir;   // editor/ (has player/ in it)
        std::filesystem::path projectDir;  // the project folder
        std::filesystem::path scriptsDir;  // its scripts/ folder
        std::filesystem::path workDir;     // where each platform is built
        std::filesystem::path depsDir;     // raylib etc. sources (the editor's own copies)
        std::string cmake;                 // the cmake program
        std::string generator;             // the editor's own CMake generator and Visual Studio
        std::string generatorInstance;     // (Windows: exports use the same one)
    };
    struct Line {
        std::string text;
        int kind = 0;  // 0 progress, 1 success, 2 error, 3 heading
    };

    explicit Exporter(Paths paths);
    ~Exporter();
    Exporter(const Exporter&) = delete;
    Exporter& operator=(const Exporter&) = delete;

    // Which platforms can be built on this computer, and why not.
    std::vector<ExportTarget> detectTargets() const;

    // Installs and activates Emscripten in an emsdk folder (emsdk install/activate
    // latest) in the background, like an export; false if something is running.
    bool setUpEmsdk(const std::filesystem::path& emsdk);

    // Starts exporting in the background; false if one is already running.
    bool start(const std::vector<std::string>& targetIds, const std::string& gameName,
               const std::filesystem::path& outDir);
    bool busy() const;
    std::string currentLabel() const;
    std::vector<Line> takeLines();  // progress since the last call
    void wait();                    // until the export finishes

    // What a running export shares with its thread. When the project is closed
    // mid-export, the thread is let go and keeps working; the next Exporter (the
    // next project's) passes its progress on to the Output panel.
    struct State;

private:
    static bool exportOne(State& s, const ExportTarget& target, const std::string& program,
                          const std::string& gameName, const std::filesystem::path& outDir);
    static void say(State& s, const std::string& text, int kind = 0);
    void say(const std::string& text, int kind = 0) { say(*s_, text, kind); }

    std::shared_ptr<State> s_;
    std::thread thread_;
};

// Emscripten's SDK (emsdk), for web exports: the folder picked in the Export
// window, else $EMSDK, else a usual place (home, Desktop, Documents, C:\emsdk).
// Empty if there isn't one.
std::filesystem::path findEmsdk();
void setEmsdkFolder(const std::filesystem::path& folder);  // remembered for this user
bool isEmsdkFolder(const std::filesystem::path& folder);    // has emsdk.py in it
bool emsdkInstalled(const std::filesystem::path& emsdk);    // `emsdk install/activate` was done

// A file-name-safe version of the game's name: "My Game!" -> "My_Game".
std::string programName(const std::string& gameName);

}  // namespace s2d
