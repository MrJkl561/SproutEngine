// The few operating system calls hot reloading needs: loading a library at
// run time, and running a command. Kept apart from everything else because
// <windows.h> and raylib.h can't be included in the same file (both declare
// names like DrawText and Rectangle).
#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace s2d::platform {

// LoadLibrary / dlopen. Returns nullptr and fills error if it fails.
void* loadLibrary(const std::filesystem::path& file, std::string& error);
void freeLibrary(void* library);

int processId();

// Runs a command line, waits for it, and returns its exit code. Everything it
// printed (stdout and stderr) ends up in output.
int runCommand(const std::string& command, std::string& output);
// The same, but also hands each line to onLine as soon as it's printed.
int runCommand(const std::string& command, std::string& output, const std::function<void(const std::string&)>& onLine);

// The system's own "choose a folder" window (Explorer on Windows, Finder's on
// macOS, zenity or kdialog on Linux). Waits until it's closed; nullopt if it
// was cancelled. owner is the editor's window (Windows; may be null).
bool canPickFolder();
std::optional<std::filesystem::path> pickFolder(const std::string& title, const std::filesystem::path& start,
                                                void* owner);

// Starts a command line (run by cmd.exe on Windows, sh elsewhere) and returns
// at once, leaving it running after the editor closes. False if it couldn't start.
bool launch(const std::string& command);
// A file or folder name quoted for that command line.
std::string quoteArg(const std::filesystem::path& path);
// Opens a file with the program the system uses for it (like double-clicking it).
bool openWithSystem(const std::filesystem::path& file);
// Shows the file selected in Explorer / Finder (Linux: opens its folder).
bool showInFileManager(const std::filesystem::path& file);

// Finds a program on the PATH (adding .exe/.bat/.cmd on Windows); "" if it isn't there.
std::filesystem::path findProgram(const std::string& name);

}  // namespace s2d::platform
