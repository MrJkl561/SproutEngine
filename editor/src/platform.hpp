// The few operating system calls hot reloading needs: loading a library at
// run time, and running a command. Kept apart from everything else because
// <windows.h> and raylib.h can't be included in the same file (both declare
// names like DrawText and Rectangle).
#pragma once

#include <filesystem>
#include <functional>
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

// Finds a program on the PATH (adding .exe/.bat/.cmd on Windows); "" if it isn't there.
std::filesystem::path findProgram(const std::string& name);

}  // namespace s2d::platform
