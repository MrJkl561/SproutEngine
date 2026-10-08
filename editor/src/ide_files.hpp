// Setup files for other code editors, written into the project folder so
// VS Code, Visual Studio, CLion, Zed, Sublime Text, Kate, Neovim... find
// script.hpp and raylib when you open the project there:
//
//   compile_commands.json         every clangd-based editor, and CLion
//   .vscode/c_cpp_properties.json VS Code's C/C++ extension
//   .vscode/settings.json         VS Code's clangd extension: uses the editor's own clangd
//   CppProperties.json            Visual Studio's "Open Folder" (Windows only)
//
// They hold paths on this computer, so they're added to the project's
// .gitignore and rewritten each time the editor opens the project. Settings of
// your own in the .vscode files and CppProperties.json are kept.
#pragma once

#include <filesystem>
#include <string>

namespace s2d {

struct IdeSetup {
    std::filesystem::path project;    // the project folder
    std::filesystem::path editorSrc;  // editor/src (script.hpp...)
    std::filesystem::path depsDir;    // raylib-src, raylib_cpp-src
    std::filesystem::path compiler;   // the C++ compiler the scripts are built with
    std::string compilerId;           // CMake's: GNU, Clang, AppleClang, MSVC
    std::filesystem::path clangd;     // the editor's clangd ("" = none)
};

// Writes (or refreshes) the files above. Returns false and sets error (one
// line per problem) if something couldn't be written or a file of yours had to
// be left alone; files that are already up to date aren't touched.
bool writeIdeFiles(const IdeSetup& setup, std::string& error);

}  // namespace s2d
