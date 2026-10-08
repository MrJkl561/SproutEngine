// Where the editor's own files are: its source code (scripts are compiled
// against it), the libraries' sources, CMake, the theme, the demo project...
//
// Found when the editor starts, so it keeps working when it's moved:
//   - a portable copy (scripts/make-portable) has everything next to the
//     program: editor/, cmake/ and deps/ beside sprout_editor(.exe);
//   - a build in the Sprout folder uses the places it was built with, or, if
//     the Sprout folder has been moved since, finds them again from where the
//     program is now.
#pragma once

#include <filesystem>
#include <string>

namespace s2d {

struct EditorPaths {
    std::filesystem::path editorDir;    // editor/: src/, scripts_module/, player/, templates/, project/
    std::filesystem::path srcDir;       // editor/src (script.hpp, nodes.hpp, ...)
    std::filesystem::path depsDir;      // raylib-src, raylib_cpp-src, ... (the libraries' sources)
    std::filesystem::path buildDir;     // where each project's script and export builds go
    std::filesystem::path themeDir;     // the theme's pictures and fonts
    std::filesystem::path font;         // the default UI font ("" if none)
    std::filesystem::path demoProject;  // editor/project
    std::filesystem::path cmake;        // "" if CMake can't be found
    std::string generator;              // the CMake generator the editor was built with
    std::string generatorInstance;      // its Visual Studio folder, "" if gone or none
    std::filesystem::path clangd;       // "" if none
    std::filesystem::path cxx;          // the compiler the editor was built with
    std::string cxxId;                  // GNU, Clang, MSVC...
    std::filesystem::path implib;       // Windows: the editor's import library, for scripts to link to
    bool portable = false;              // files found next to the program
};

const EditorPaths& editorPaths();

}  // namespace s2d
