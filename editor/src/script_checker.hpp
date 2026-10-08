// Checks the scripts open in the Script tab while you type, before saving:
// errors and warnings for each line, and what every name is (a member, a
// parameter, a type...) so the editor can color it.
//
// It uses clangd (downloaded with the editor, see editor/CMakeLists.txt). Where
// there's no clangd it runs the compiler in "just check" mode instead (errors
// only). Everything runs in the background; the editor polls for results.
#pragma once

#include <atomic>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "text_editor.hpp"

namespace s2d {

namespace platform {
class ChildProcess;
}

class ScriptChecker {
public:
    struct Setup {
        std::filesystem::path clangd;      // "" = none
        std::filesystem::path compiler;    // the C++ compiler (the fallback)
        std::string compilerId;            // CMake's: GNU, Clang, AppleClang, MSVC
        std::filesystem::path scriptsDir;  // the project's scripts/
        std::filesystem::path workDir;     // a folder of the project's own for its files
        std::filesystem::path editorSrc;   // editor/src (script.hpp...)
        std::filesystem::path depsDir;     // raylib-src, raylib_cpp-src
    };
    // What a name is, for coloring
    using TokenKind = SemanticKind;
    using Token = SemanticSpan;
    struct Result {
        long version = 0;  // the document version these are for
        bool hasDiagnostics = false, hasTokens = false;
        std::vector<Diagnostic> diagnostics;
        std::vector<Token> tokens;
    };

    explicit ScriptChecker(Setup setup);
    ~ScriptChecker();  // quick: never waits for a check to finish

    // "clangd", "compiler" or "" (nothing to check with)
    std::string engine() const;
    bool busy() const;  // a check is running or waiting

    // The text of an open script (call when it changed); version = TextDocument's.
    void update(const std::filesystem::path& file, const std::string& text, long version);
    void close(const std::filesystem::path& file);
    void poll(double now);  // sends changes once typing pauses
    // New results for a file since the last call.
    bool take(const std::filesystem::path& file, Result& out);

    struct Shared;  // what the background threads share (internal)

private:
    void writeCompileCommands() const;
    void startClangd();
    void send(const std::string& json);
    void runCompilerCheck(const std::string& key, long version, const std::string& text);

    Setup setup_;
    std::shared_ptr<Shared> shared_;
    std::shared_ptr<platform::ChildProcess> clangd_;
    std::thread reader_;
    std::string engine_;
    int nextId_ = 2;  // 1 is initialize

    struct Doc {
        std::filesystem::path path;
        std::string uri;
        std::string text;
        long version = 0;
        double changedAt = 0;
        bool pending = false, opened = false;
    };
    std::map<std::string, Doc> docs_;  // by path
};

}  // namespace s2d
