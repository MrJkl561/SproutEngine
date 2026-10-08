#include "platform.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#else
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
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

std::filesystem::path executablePath() {
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        DWORD n = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (n == 0) return {};
        if (n < buffer.size()) return std::filesystem::path(buffer.substr(0, n));
        buffer.resize(buffer.size() * 2);
    }
}

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

std::wstring widen(const std::string& text) {
    int n = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring out(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), n);
    return out;
}

bool launch(const std::string& command) {
    // cmd.exe /s /c "..." runs the line as typed (and finds .cmd/.bat programs like code.cmd).
    std::wstring line = L"cmd.exe /d /s /c \"" + widen(command) + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr,
                        nullptr, &si, &pi))
        return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

std::string quoteArg(const std::filesystem::path& path) { return "\"" + path.string() + "\""; }

bool openWithSystem(const std::filesystem::path& file) {
    auto result = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"open", file.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (result > 32) return true;
    // No program is set for this kind of file: let Windows ask which one to use.
    result = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"openas", file.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    return result > 32;
}

bool showInFileManager(const std::filesystem::path& file) {
    return launch("explorer /select," + quoteArg(file.lexically_normal().make_preferred()));
}

bool moveToTrash(const std::filesystem::path& file, std::string& error) {
    std::wstring from = std::filesystem::absolute(file).lexically_normal().make_preferred().wstring();
    from.push_back(L'\0');  // the list of files ends with two nulls
    SHFILEOPSTRUCTW op{};
    op.wFunc = FO_DELETE;
    op.pFrom = from.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
    int result = SHFileOperationW(&op);
    if (result != 0 || op.fAnyOperationsAborted) {
        error = "Windows couldn't move it to the Recycle Bin (error " + std::to_string(result) + ")";
        return false;
    }
    return true;
}

namespace {
class WinProcess : public ChildProcess {
public:
    HANDLE process = nullptr, in = nullptr, out = nullptr;
    void terminate() override {
        if (in) CloseHandle(in);  // clangd ends when its input closes
        in = nullptr;
        if (process && WaitForSingleObject(process, 300) == WAIT_TIMEOUT) TerminateProcess(process, 0);
    }
    ~WinProcess() override {
        terminate();
        if (out) CloseHandle(out);
        if (process) CloseHandle(process);
    }
    bool write(const std::string& data) override {
        if (!in) return false;
        DWORD written = 0;
        return WriteFile(in, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) && written == data.size();
    }
    size_t read(char* buffer, size_t size) override {
        DWORD got = 0;
        if (!ReadFile(out, buffer, static_cast<DWORD>(size), &got, nullptr)) return 0;
        return got;
    }
};

std::wstring argQuote(const std::wstring& a) {
    if (!a.empty() && a.find_first_of(L" \t\"") == std::wstring::npos) return a;
    std::wstring out = L"\"";
    for (size_t i = 0; i < a.size(); ++i) {
        size_t slashes = 0;
        while (i < a.size() && a[i] == L'\\') { ++slashes; ++i; }
        if (i == a.size()) { out.append(slashes * 2, L'\\'); break; }
        if (a[i] == L'"') { out.append(slashes * 2 + 1, L'\\'); out += L'"'; }
        else { out.append(slashes, L'\\'); out += a[i]; }
    }
    return out + L"\"";
}
}  // namespace

std::unique_ptr<ChildProcess> startProcess(const std::vector<std::string>& args, std::string& error) {
    SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
    HANDLE inRead = nullptr, inWrite = nullptr, outRead = nullptr, outWrite = nullptr;
    if (!CreatePipe(&inRead, &inWrite, &sa, 0) || !CreatePipe(&outRead, &outWrite, &sa, 0)) {
        error = "couldn't make pipes";
        return nullptr;
    }
    SetHandleInformation(inWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    std::wstring line;
    for (const std::string& a : args) line += (line.empty() ? L"" : L" ") + argQuote(widen(a));
    STARTUPINFOW si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = inRead;
    si.hStdOutput = outWrite;
    si.hStdError = nul;
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(inRead);
    CloseHandle(outWrite);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!ok) {
        CloseHandle(inWrite);
        CloseHandle(outRead);
        error = "couldn't start " + args[0];
        return nullptr;
    }
    CloseHandle(pi.hThread);
    auto p = std::make_unique<WinProcess>();
    p->process = pi.hProcess;
    p->in = inWrite;
    p->out = outRead;
    return p;
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

std::filesystem::path executablePath() {
    std::error_code ec;
#if defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) return {};
    std::filesystem::path p = std::filesystem::weakly_canonical(buffer.c_str(), ec);
    return ec ? std::filesystem::path(buffer.c_str()) : p;
#else
    std::filesystem::path p = std::filesystem::read_symlink("/proc/self/exe", ec);
    return ec ? std::filesystem::path() : p;
#endif
}

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

bool launch(const std::string& command) {
    // Fork twice so the program isn't the editor's child (no zombie, and it
    // keeps running when the editor quits).
    pid_t child = fork();
    if (child < 0) return false;
    if (child == 0) {
        setsid();
        if (fork() == 0) {
            if (FILE* null = std::fopen("/dev/null", "r+")) {
                dup2(fileno(null), 0);
                dup2(fileno(null), 1);
                dup2(fileno(null), 2);
            }
            execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char*>(nullptr));
            _exit(127);
        }
        _exit(0);
    }
    int status = 0;
    waitpid(child, &status, 0);
    return true;
}

std::string quoteArg(const std::filesystem::path& path) { return shellQuote(path.string()); }

bool openWithSystem(const std::filesystem::path& file) {
#if defined(__APPLE__)
    return launch("open " + quoteArg(file));
#else
    return launch("xdg-open " + quoteArg(file));
#endif
}

bool showInFileManager(const std::filesystem::path& file) {
#if defined(__APPLE__)
    return launch("open -R " + quoteArg(file));
#else
    return launch("xdg-open " + quoteArg(file.parent_path()));
#endif
}

namespace {
// Moves `from` to `to` (also across drives: copy, then delete)
bool moveFile(const std::filesystem::path& from, const std::filesystem::path& to, std::string& error) {
    std::error_code ec;
    std::filesystem::rename(from, to, ec);
    if (!ec) return true;
    std::filesystem::copy(from, to, std::filesystem::copy_options::recursive, ec);
    if (ec) {
        error = ec.message();
        return false;
    }
    std::filesystem::remove_all(from, ec);
    return true;
}

// name, or "name 2", "name 3"... whichever isn't in dir yet
std::filesystem::path freeName(const std::filesystem::path& dir, const std::filesystem::path& name) {
    std::error_code ec;
    std::filesystem::path p = dir / name;
    for (int i = 2; std::filesystem::exists(p, ec); ++i)
        p = dir / (name.stem().string() + " " + std::to_string(i) + name.extension().string());
    return p;
}
}  // namespace

bool moveToTrash(const std::filesystem::path& file, std::string& error) {
    std::error_code ec;
    std::filesystem::path full = std::filesystem::absolute(file, ec).lexically_normal();
    const char* home = std::getenv("HOME");
    if (!home || !*home) {
        error = "no home folder (HOME isn't set)";
        return false;
    }
#if defined(__APPLE__)
    std::filesystem::path trash = std::filesystem::path(home) / ".Trash";
    std::filesystem::create_directories(trash, ec);
    return moveFile(full, freeName(trash, full.filename()), error);
#else
    // The freedesktop.org Trash (what the desktop's file manager uses):
    // files/ holds the file, info/<name>.trashinfo where it came from.
    const char* data = std::getenv("XDG_DATA_HOME");
    std::filesystem::path trash =
        (data && *data ? std::filesystem::path(data) : std::filesystem::path(home) / ".local" / "share") / "Trash";
    std::filesystem::create_directories(trash / "files", ec);
    std::filesystem::create_directories(trash / "info", ec);
    std::filesystem::path to = freeName(trash / "files", full.filename());
    std::string encoded;  // the path, percent-encoded
    for (unsigned char c : full.string()) {
        if (std::isalnum(c) || std::strchr("/-_.~", c)) {
            encoded += static_cast<char>(c);
        } else {
            char hex[4];
            std::snprintf(hex, sizeof hex, "%%%02X", c);
            encoded += hex;
        }
    }
    char when[32];
    std::time_t now = std::time(nullptr);
    std::strftime(when, sizeof when, "%Y-%m-%dT%H:%M:%S", std::localtime(&now));
    {
        std::ofstream info(trash / "info" / (to.filename().string() + ".trashinfo"));
        info << "[Trash Info]\nPath=" << encoded << "\nDeletionDate=" << when << "\n";
    }
    if (!moveFile(full, to, error)) {
        std::filesystem::remove(trash / "info" / (to.filename().string() + ".trashinfo"), ec);
        return false;
    }
    return true;
#endif
}

namespace {
class PosixProcess : public ChildProcess {
public:
    pid_t pid = -1;
    int in = -1, out = -1;
    void terminate() override {
        if (in >= 0) close(in);  // clangd ends when its input closes
        in = -1;
        if (pid > 0) {
            for (int i = 0; i < 30; ++i) {  // up to 0.3 s
                if (waitpid(pid, nullptr, WNOHANG) == pid) { pid = -1; break; }
                usleep(10000);
            }
            if (pid > 0) {
                kill(pid, SIGKILL);
                waitpid(pid, nullptr, 0);
                pid = -1;
            }
        }
    }
    ~PosixProcess() override {
        terminate();
        if (out >= 0) close(out);
    }
    bool write(const std::string& data) override {
        if (in < 0) return false;
        size_t done = 0;
        while (done < data.size()) {
            ssize_t n = ::write(in, data.data() + done, data.size() - done);
            if (n <= 0) return false;
            done += static_cast<size_t>(n);
        }
        return true;
    }
    size_t read(char* buffer, size_t size) override {
        ssize_t n = ::read(out, buffer, size);
        return n > 0 ? static_cast<size_t>(n) : 0;
    }
};
}  // namespace

std::unique_ptr<ChildProcess> startProcess(const std::vector<std::string>& args, std::string& error) {
    int toChild[2], fromChild[2];
    if (pipe(toChild) != 0 || pipe(fromChild) != 0) {
        error = "couldn't make pipes";
        return nullptr;
    }
    signal(SIGPIPE, SIG_IGN);  // writing to a program that ended must not end the editor
    pid_t pid = fork();
    if (pid < 0) {
        error = "couldn't start " + args[0];
        return nullptr;
    }
    if (pid == 0) {
        dup2(toChild[0], 0);
        dup2(fromChild[1], 1);
        int null = open("/dev/null", O_WRONLY);
        if (null >= 0) dup2(null, 2);
        close(toChild[1]);
        close(fromChild[0]);
        std::vector<char*> argv;
        for (const std::string& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    close(toChild[0]);
    close(fromChild[1]);
    fcntl(toChild[1], F_SETFD, FD_CLOEXEC);
    fcntl(fromChild[0], F_SETFD, FD_CLOEXEC);
    auto p = std::make_unique<PosixProcess>();
    p->pid = pid;
    p->in = toChild[1];
    p->out = fromChild[0];
    return p;
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
