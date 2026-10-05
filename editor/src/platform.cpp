#include "platform.hpp"

#include <cstdio>
#include <cstdlib>

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
#include <sys/wait.h>
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

namespace {
// cmd.exe drops the first and last quote of a command line that starts
// with one, so wrap the whole thing in an extra pair.
FILE* openPipe(const std::string& command) { return _popen(("\"" + command + " 2>&1\"").c_str(), "r"); }
int closePipe(FILE* pipe) { return _pclose(pipe); }
const char* kPathSeparator = ";";
const char* kExtensions[] = {".exe", ".bat", ".cmd", ""};
}  // namespace

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

namespace {
FILE* openPipe(const std::string& command) { return popen((command + " 2>&1").c_str(), "r"); }
int closePipe(FILE* pipe) {
    int status = pclose(pipe);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
const char* kPathSeparator = ":";
const char* kExtensions[] = {""};
}  // namespace

#endif

int runCommand(const std::string& command, std::string& output) { return runCommand(command, output, {}); }

int runCommand(const std::string& command, std::string& output, const std::function<void(const std::string&)>& onLine) {
    FILE* pipe = openPipe(command);
    if (!pipe) {
        output = "couldn't run: " + command;
        return -1;
    }
    char buf[1024];
    std::string line;
    while (fgets(buf, sizeof buf, pipe)) {
        output += buf;
        line += buf;
        if (!line.empty() && line.back() == '\n') {
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
            if (onLine) onLine(line);
            line.clear();
        }
    }
    if (!line.empty() && onLine) onLine(line);
    return closePipe(pipe);
}

std::filesystem::path findProgram(const std::string& name) {
    const char* path = std::getenv("PATH");
    if (!path) return {};
    std::string all = path;
    size_t start = 0;
    while (start <= all.size()) {
        size_t end = all.find(kPathSeparator, start);
        std::string dir = all.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!dir.empty()) {
            for (const char* ext : kExtensions) {
                std::filesystem::path candidate = std::filesystem::path(dir) / (name + ext);
                std::error_code ec;
                if (std::filesystem::is_regular_file(candidate, ec)) return candidate;
            }
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return {};
}

}  // namespace s2d::platform
