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

    // Starts exporting in the background; false if one is already running.
    bool start(const std::vector<std::string>& targetIds, const std::string& gameName,
               const std::filesystem::path& outDir);
    bool busy() const { return busy_; }
    std::string currentLabel() const;
    std::vector<Line> takeLines();  // progress since the last call
    void wait();                    // until the export finishes

private:
    bool exportOne(const ExportTarget& target, const std::string& program, const std::string& gameName,
                   const std::filesystem::path& outDir);
    void say(const std::string& text, int kind = 0);

    Paths paths_;
    std::thread thread_;
    std::atomic<bool> busy_{false};
    mutable std::mutex mutex_;
    std::vector<Line> lines_;
    std::string current_;
};

// A file-name-safe version of the game's name: "My Game!" -> "My_Game".
std::string programName(const std::string& gameName);

}  // namespace s2d
