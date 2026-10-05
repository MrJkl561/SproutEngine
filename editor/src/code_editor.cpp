#include "code_editor.hpp"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <fstream>

#include "platform.hpp"
#include "projects.hpp"

namespace fs = std::filesystem;

namespace s2d {

namespace {

// One way to start an editor: it's used if `check` is a program on the PATH,
// or an existing file or folder (an absolute path).
struct Candidate {
    std::string name, check, command;
};

[[maybe_unused]] std::string q(const fs::path& p) { return platform::quoteArg(p); }

bool available(const std::string& check) {
    if (check.empty()) return true;
    std::error_code ec;
    if (fs::path(check).is_absolute()) return fs::exists(check, ec);
    return !platform::findProgram(check).empty();
}

std::vector<Candidate> candidates() {
    std::vector<Candidate> list;
#if defined(_WIN32)
    auto env = [](const char* name) { const char* v = std::getenv(name); return std::string(v ? v : ""); };
    std::string programs = env("ProgramFiles"), programsX86 = env("ProgramFiles(x86)"), local = env("LOCALAPPDATA");
    list.push_back({"VS Code", "code", "code {project} {file}"});
    if (!local.empty()) {
        fs::path exe = fs::path(local) / "Programs/Microsoft VS Code/Code.exe";
        list.push_back({"VS Code", exe.string(), q(exe.make_preferred()) + " {project} {file}"});
    }
    // Visual Studio: ask its installer where the newest one is.
    if (!programsX86.empty()) {
        fs::path vswhere = fs::path(programsX86) / "Microsoft Visual Studio/Installer/vswhere.exe";
        std::error_code ec;
        if (fs::exists(vswhere, ec)) {
            std::string out;
            platform::runCommand(q(vswhere.make_preferred()) + " -latest -property productPath", out);
            while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ')) out.pop_back();
            if (!out.empty() && fs::exists(out, ec))
                list.push_back({"Visual Studio", out, q(out) + " /Edit {file}"});
        }
    }
    list.push_back({"Visual Studio", "devenv", "devenv /Edit {file}"});
    list.push_back({"CLion", "clion64", "clion64 {file}"});
    list.push_back({"CLion", "clion", "clion {file}"});
    list.push_back({"Sublime Text", "subl", "subl {project} {file}"});
    if (!programs.empty()) {
        fs::path subl = fs::path(programs) / "Sublime Text/subl.exe";
        list.push_back({"Sublime Text", subl.string(), q(subl.make_preferred()) + " {project} {file}"});
        fs::path npp = fs::path(programs) / "Notepad++/notepad++.exe";
        list.push_back({"Notepad++", npp.string(), q(npp.make_preferred()) + " {file}"});
    }
    list.push_back({"Notepad++", "notepad++", "notepad++ {file}"});
    list.push_back({"Zed", "zed", "zed {project} {file}"});
    list.push_back({"Notepad", "notepad", "notepad {file}"});
#elif defined(__APPLE__)
    auto app = [](const std::string& name) {
        std::error_code ec;
        for (fs::path dir : {fs::path("/Applications"), fs::path(std::getenv("HOME") ? std::getenv("HOME") : "") / "Applications"})
            if (fs::exists(dir / (name + ".app"), ec)) return (dir / (name + ".app")).string();
        return std::string("/Applications/" + name + ".app");
    };
    list.push_back({"VS Code", "code", "code {project} {file}"});
    list.push_back({"VS Code", app("Visual Studio Code"), "open -a 'Visual Studio Code' {project} {file}"});
    list.push_back({"Xcode", app("Xcode"), "open -a Xcode {file}"});
    list.push_back({"CLion", "clion", "clion {file}"});
    list.push_back({"CLion", app("CLion"), "open -a CLion {file}"});
    list.push_back({"Sublime Text", "subl", "subl {project} {file}"});
    list.push_back({"Sublime Text", app("Sublime Text"), "open -a 'Sublime Text' {project} {file}"});
    list.push_back({"Zed", "zed", "zed {project} {file}"});
    list.push_back({"Zed", app("Zed"), "open -a Zed {project} {file}"});
    list.push_back({"TextEdit", app("TextEdit"), "open -e {file}"});
#else
    list.push_back({"VS Code", "code", "code {project} {file}"});
    list.push_back({"VSCodium", "codium", "codium {project} {file}"});
    list.push_back({"CLion", "clion", "clion {file}"});
    list.push_back({"CLion", "clion.sh", "clion.sh {file}"});
    list.push_back({"Sublime Text", "subl", "subl {project} {file}"});
    list.push_back({"Zed", "zed", "zed {project} {file}"});
    list.push_back({"Zed", "zeditor", "zeditor {project} {file}"});
    list.push_back({"Kate", "kate", "kate {file}"});
    list.push_back({"GNOME Text Editor", "gnome-text-editor", "gnome-text-editor {file}"});
    list.push_back({"gedit", "gedit", "gedit {file}"});
    list.push_back({"Geany", "geany", "geany {file}"});
    list.push_back({"Mousepad", "mousepad", "mousepad {file}"});
#endif
    return list;
}

fs::path settingsFile() { return projects::configDir() / "code_editor.json"; }

void replaceAll(std::string& text, const std::string& from, const std::string& to) {
    for (size_t pos = 0; (pos = text.find(from, pos)) != std::string::npos; pos += to.size()) text.replace(pos, from.size(), to);
}

}  // namespace

const std::vector<CodeEditor>& installedCodeEditors() {
    static const std::vector<CodeEditor> list = [] {
        std::vector<CodeEditor> out{CodeEditor{}};
        for (const Candidate& c : candidates()) {
            bool have = false;
            for (const CodeEditor& e : out) have = have || e.name == c.name;
            if (!have && available(c.check)) out.push_back({c.name, c.command});
        }
        return out;
    }();
    return list;
}

bool codeEditorChosen() {
    std::error_code ec;
    return fs::exists(settingsFile(), ec);
}

CodeEditor loadCodeEditor() {
    CodeEditor e;
    std::ifstream in(settingsFile());
    if (!in) return e;
    try {
        nlohmann::json j = nlohmann::json::parse(in);
        e.name = j.value("name", e.name);
        e.command = j.value("command", e.command);
    } catch (const nlohmann::json::exception&) {
    }
    return e;
}

void saveCodeEditor(const CodeEditor& editor) {
    std::error_code ec;
    fs::create_directories(projects::configDir(), ec);
    std::ofstream(settingsFile()) << nlohmann::json{{"name", editor.name}, {"command", editor.command}}.dump(2) << '\n';
}

bool openInCodeEditor(const CodeEditor& editor, const fs::path& file, const fs::path& project, std::string& error) {
    std::error_code ec;
    fs::path full = fs::absolute(file, ec);
    if (!fs::exists(full, ec)) {
        error = full.string() + " doesn't exist";
        return false;
    }
    if (editor.command.empty()) {
        if (platform::openWithSystem(full)) return true;
        error = "the system has no program for " + full.extension().string() + " files";
        return false;
    }
    std::string command = editor.command;
    fs::path folder = fs::absolute(project, ec);
#if defined(_WIN32)
    full.make_preferred();
    folder.make_preferred();
#endif
    replaceAll(command, "{file}", platform::quoteArg(full));
    replaceAll(command, "{project}", platform::quoteArg(folder));
    if (command.find(platform::quoteArg(full)) == std::string::npos) command += " " + platform::quoteArg(full);
    if (platform::launch(command)) return true;
    error = "couldn't run: " + command;
    return false;
}

}  // namespace s2d
