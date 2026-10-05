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
#include <shobjidl.h>
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

bool canPickFolder() { return true; }

std::optional<std::filesystem::path> pickFolder(const std::string& title, const std::filesystem::path& start,
                                                void* owner) {
    // The standard Explorer "Select Folder" window (IFileOpenDialog).
    HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    std::optional<std::filesystem::path> result;
    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        int n = MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, nullptr, 0);
        std::wstring wideTitle(n > 0 ? n - 1 : 0, L'\0');
        if (n > 1) MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, wideTitle.data(), n);
        dialog->SetTitle(wideTitle.c_str());
        std::error_code ec;
        if (!start.empty() && std::filesystem::is_directory(start, ec)) {
            IShellItem* folder = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(std::filesystem::absolute(start, ec).wstring().c_str(), nullptr,
                                                      IID_PPV_ARGS(&folder)))) {
                dialog->SetFolder(folder);
                folder->Release();
            }
        }
        if (SUCCEEDED(dialog->Show(static_cast<HWND>(owner)))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                    result = std::filesystem::path(path);
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }
        dialog->Release();
    }
    if (SUCCEEDED(init)) CoUninitialize();
    return result;
}

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

// '...' for the shell, with any ' inside escaped.
std::string shellQuote(const std::string& text) {
    std::string out = "'";
    for (char c : text) out += c == '\'' ? std::string("'\\''") : std::string(1, c);
    return out + "'";
}

// Runs a command and returns what it printed to stdout (stderr is ignored),
// or nullopt if it failed or was cancelled.
std::optional<std::string> readCommand(const std::string& command) {
    FILE* pipe = popen((command + " 2>/dev/null").c_str(), "r");
    if (!pipe) return std::nullopt;
    std::string out;
    char buf[1024];
    while (fgets(buf, sizeof buf, pipe)) out += buf;
    int status = pclose(pipe);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return std::nullopt;
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    if (out.empty()) return std::nullopt;
    return out;
}

}  // namespace

bool canPickFolder() {
#if defined(__APPLE__)
    return true;
#else
    return !findProgram("zenity").empty() || !findProgram("kdialog").empty();
#endif
}

std::optional<std::filesystem::path> pickFolder(const std::string& title, const std::filesystem::path& start, void*) {
    std::error_code ec;
    std::string from = std::filesystem::is_directory(start, ec) ? std::filesystem::absolute(start, ec).string() : "";
    std::optional<std::string> picked;
#if defined(__APPLE__)
    // Finder's "Choose a Folder" window, through AppleScript.
    auto text = [](const std::string& t) {  // an AppleScript string
        std::string out = "\"";
        for (char c : t) out += (c == '"' || c == '\\') ? std::string("\\") + c : std::string(1, c);
        return out + "\"";
    };
    std::string script = "POSIX path of (choose folder with prompt " + text(title);
    if (!from.empty()) script += " default location POSIX file " + text(from);
    script += ")";
    picked = readCommand("osascript -e " + shellQuote(script));
    if (picked && picked->size() > 1 && picked->back() == '/') picked->pop_back();
#else
    if (!findProgram("zenity").empty()) {
        std::string cmd = "zenity --file-selection --directory --title=" + shellQuote(title);
        if (!from.empty()) cmd += " --filename=" + shellQuote(from + "/");
        picked = readCommand(cmd);
    } else if (!findProgram("kdialog").empty()) {
        picked = readCommand("kdialog --getexistingdirectory " + shellQuote(from.empty() ? "." : from) +
                             " --title " + shellQuote(title));
    }
#endif
    if (!picked) return std::nullopt;
    return std::filesystem::path(*picked);
}

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
