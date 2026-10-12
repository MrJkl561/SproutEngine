#include "editor_paths.hpp"

#include <cstdlib>
#include <utility>
#include <vector>

#include "platform.hpp"

namespace fs = std::filesystem;

// The places the editor was built with (see editor/CMakeLists.txt)
#ifndef SPROUT_EDITOR_DIR
#define SPROUT_EDITOR_DIR ""
#endif
#ifndef SPROUT_EDITOR_DEPS_DIR
#define SPROUT_EDITOR_DEPS_DIR ""
#endif
#ifndef SPROUT_EDITOR_BUILD_DIR
#define SPROUT_EDITOR_BUILD_DIR ""
#endif
#ifndef SPROUT_EDITOR_THEME_DIR
#define SPROUT_EDITOR_THEME_DIR ""
#endif
#ifndef SPROUT_EDITOR_FONT
#define SPROUT_EDITOR_FONT ""
#endif
#ifndef SPROUT_EDITOR_CMAKE
#define SPROUT_EDITOR_CMAKE ""
#endif
#ifndef SPROUT_EDITOR_GENERATOR
#define SPROUT_EDITOR_GENERATOR ""
#endif
#ifndef SPROUT_EDITOR_GENERATOR_INSTANCE
#define SPROUT_EDITOR_GENERATOR_INSTANCE ""
#endif
#ifndef SPROUT_EDITOR_CLANGD
#define SPROUT_EDITOR_CLANGD ""
#endif
#ifndef SPROUT_EDITOR_CXX
#define SPROUT_EDITOR_CXX ""
#endif
#ifndef SPROUT_EDITOR_CXX_ID
#define SPROUT_EDITOR_CXX_ID ""
#endif
#ifndef SPROUT_EDITOR_IMPLIB
#define SPROUT_EDITOR_IMPLIB ""
#endif

namespace s2d {

namespace {

bool isThere(const fs::path& p) {
    std::error_code ec;
    return !p.empty() && fs::exists(p, ec);
}

// A folder with the editor's sources in it (editor/ in the Sprout folder)
bool isEditorDir(const fs::path& dir) {
    return isThere(dir / "src" / "script.hpp") && isThere(dir / "scripts_module" / "CMakeLists.txt");
}

// p moved from under `from` to under `to`; "" if p isn't under `from`
fs::path moved(const fs::path& p, const fs::path& from, const fs::path& to) {
    if (p.empty() || from.empty()) return {};
    fs::path rel = p.lexically_normal().lexically_relative(from.lexically_normal());
    if (rel.empty() || *rel.begin() == "..") return {};
    return rel == "." ? to : to / rel;
}

// The first folder, from `start` upwards, that passes the test
template <class Test> fs::path findUp(fs::path start, Test test) {
    for (int i = 0; i < 8 && !start.empty(); ++i) {
        if (test(start)) return start;
        fs::path parent = start.parent_path();
        if (parent == start) break;
        start = parent;
    }
    return {};
}

// Where a portable copy keeps the projects' builds: the user's cache folder,
// which can always be written to (unlike, say, Program Files).
fs::path cacheDir() {
#if defined(_WIN32)
    if (const char* p = std::getenv("LOCALAPPDATA"); p && *p) return fs::path(p) / "SproutEditor" / "build";
    if (const char* p = std::getenv("USERPROFILE"); p && *p) return fs::path(p) / "SproutEditor" / "build";
#elif defined(__APPLE__)
    if (const char* p = std::getenv("HOME"); p && *p) return fs::path(p) / "Library" / "Caches" / "SproutEditor";
#else
    if (const char* p = std::getenv("XDG_CACHE_HOME"); p && *p) return fs::path(p) / "sprout-editor";
    if (const char* p = std::getenv("HOME"); p && *p) return fs::path(p) / ".cache" / "sprout-editor";
#endif
    return fs::temp_directory_path() / "sprout-editor";
}

// CMake: the one the editor was built with, else one on the PATH or in the
// usual places (programs started from the Finder don't get the shell's PATH).
fs::path findCMake() {
    if (isThere(SPROUT_EDITOR_CMAKE)) return SPROUT_EDITOR_CMAKE;
    if (fs::path p = platform::findProgram("cmake"); !p.empty()) return p;
    std::vector<fs::path> places;
#if defined(_WIN32)
    for (const char* var : {"ProgramFiles", "ProgramW6432", "ProgramFiles(x86)"})
        if (const char* p = std::getenv(var)) places.push_back(fs::path(p) / "CMake" / "bin" / "cmake.exe");
    // The CMake that comes with Visual Studio's C++ tools
    if (const char* p = std::getenv("ProgramFiles(x86)")) {
        fs::path vswhere = fs::path(p) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe";
        if (isThere(vswhere)) {
            std::string out;
            platform::runCommand("\"" + vswhere.string() + "\" -latest -products * -property installationPath", out);
            while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ')) out.pop_back();
            if (auto nl = out.find_last_of('\n'); nl != std::string::npos) out = out.substr(nl + 1);
            if (!out.empty())
                places.push_back(fs::path(out) / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" /
                                 "CMake" / "bin" / "cmake.exe");
        }
    }
#elif defined(__APPLE__)
    places = {"/opt/homebrew/bin/cmake", "/usr/local/bin/cmake", "/Applications/CMake.app/Contents/bin/cmake"};
#else
    places = {"/usr/bin/cmake", "/usr/local/bin/cmake", "/snap/bin/cmake"};
#endif
    for (const fs::path& p : places)
        if (isThere(p)) return p;
    return {};
}

EditorPaths find() {
    EditorPaths paths;
    const fs::path bakedEditor = SPROUT_EDITOR_DIR, bakedDeps = SPROUT_EDITOR_DEPS_DIR,
                   bakedBuild = SPROUT_EDITOR_BUILD_DIR, bakedTheme = SPROUT_EDITOR_THEME_DIR,
                   bakedImplib = SPROUT_EDITOR_IMPLIB;
    const fs::path exeDir = platform::executablePath().parent_path();

    // Where things went: each (old place, new place), most specific first.
    std::vector<std::pair<fs::path, fs::path>> moves;
    if (isEditorDir(exeDir / "editor")) {
        // A portable copy: everything beside the program
        paths.portable = true;
        paths.editorDir = exeDir / "editor";
        paths.depsDir = exeDir / "deps";
        paths.buildDir = cacheDir();
        moves = {{bakedTheme, paths.editorDir / "theme"},
                 {bakedDeps, paths.depsDir},
                 {bakedEditor, paths.editorDir},
                 {bakedImplib.parent_path(), exeDir}};
    } else if (isEditorDir(bakedEditor) || exeDir.empty()) {
        // Built here, still here
        paths.editorDir = bakedEditor;
        paths.depsDir = bakedDeps;
        paths.buildDir = bakedBuild;
    } else {
        // The Sprout folder was moved after building: find it from the program
        // (it's in <build folder>/bin/sprout_editor/).
        fs::path root = findUp(exeDir, [](const fs::path& d) { return isEditorDir(d / "editor"); });
        fs::path build = findUp(exeDir, [](const fs::path& d) { return isThere(d / "CMakeCache.txt"); });
        paths.editorDir = root.empty() ? bakedEditor : root / "editor";
        paths.buildDir = build.empty() ? bakedBuild : build;
        if (!build.empty()) moves.push_back({bakedBuild, build});
        if (!root.empty()) moves.push_back({bakedEditor.parent_path(), root});
        paths.depsDir = bakedDeps;
    }
    // A file or folder in its new place, if it has one
    auto relocate = [&](const fs::path& p) -> fs::path {
        if (p.empty()) return p;
        if (!paths.portable && isThere(p)) return p;
        for (auto& [from, to] : moves)
            if (fs::path q = moved(p, from, to); isThere(q)) return q;
        return p;
    };
    if (!paths.portable) paths.depsDir = relocate(bakedDeps);
    paths.srcDir = paths.editorDir / "src";
    paths.demoProject = paths.editorDir / "project";
    paths.themeDir = relocate(bakedTheme);
    paths.font = relocate(SPROUT_EDITOR_FONT);
    paths.implib = relocate(bakedImplib);

    paths.cmake = findCMake();
    paths.generator = SPROUT_EDITOR_GENERATOR;
    if (isThere(SPROUT_EDITOR_GENERATOR_INSTANCE)) paths.generatorInstance = SPROUT_EDITOR_GENERATOR_INSTANCE;
    paths.cxx = SPROUT_EDITOR_CXX;
    if (!isThere(paths.cxx) && !paths.cxx.empty())
        if (fs::path p = platform::findProgram(paths.cxx.filename().string()); !p.empty()) paths.cxx = p;
    paths.cxxId = SPROUT_EDITOR_CXX_ID;
    // (none if the editor was built without one: then scripts are checked with the compiler)
    paths.clangd = relocate(SPROUT_EDITOR_CLANGD);
    if (!paths.clangd.empty() && !isThere(paths.clangd)) paths.clangd = platform::findProgram("clangd");
    return paths;
}

}  // namespace

const EditorPaths& editorPaths() {
    static const EditorPaths paths = find();
    return paths;
}

}  // namespace s2d
