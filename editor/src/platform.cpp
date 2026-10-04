#include "platform.hpp"

#include <cstdio>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace s2d::platform {

#if defined(_WIN32)

void* loadLibrary(const std::filesystem::path& file, std::string& error) {
    HMODULE module = LoadLibraryW(file.wstring().c_str());
    if (!module) {
        DWORD code = GetLastError();
        char* text = nullptr;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr, code, 0, reinterpret_cast<char*>(&text), 0, nullptr);
        error = "LoadLibrary failed (" + std::to_string(code) + ")" + (text ? std::string(": ") + text : "");
        if (text) LocalFree(text);
        while (!error.empty() && (error.back() == '\n' || error.back() == '\r')) error.pop_back();
    }
    return module;
}

void freeLibrary(void* library) {
    if (library) FreeLibrary(static_cast<HMODULE>(library));
}

int processId() { return static_cast<int>(GetCurrentProcessId()); }

int runCommand(const std::string& command, std::string& output) {
    // cmd.exe drops the first and last quote of a command line that starts
    // with one, so wrap the whole thing in an extra pair.
    std::string line = "\"" + command + " 2>&1\"";
    FILE* pipe = _popen(line.c_str(), "r");
    if (!pipe) {
        output = "couldn't run: " + command;
        return -1;
    }
    char buf[512];
    while (fgets(buf, sizeof buf, pipe)) output += buf;
    return _pclose(pipe);
}

#else

void* loadLibrary(const std::filesystem::path& file, std::string& error) {
    void* handle = dlopen(file.string().c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        const char* why = dlerror();
        error = why ? why : "dlopen failed";
    }
    return handle;
}

void freeLibrary(void* library) {
    if (library) dlclose(library);
}

int processId() { return static_cast<int>(getpid()); }

int runCommand(const std::string& command, std::string& output) {
    FILE* pipe = popen((command + " 2>&1").c_str(), "r");
    if (!pipe) {
        output = "couldn't run: " + command;
        return -1;
    }
    char buf[512];
    while (fgets(buf, sizeof buf, pipe)) output += buf;
    return pclose(pipe);
}

#endif

}  // namespace s2d::platform
