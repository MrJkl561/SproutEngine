#include "projects.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>

#include <nlohmann/json.hpp>

#include "project.hpp"

namespace fs = std::filesystem;
using nlohmann::json;

namespace s2d::projects {

namespace {

fs::path home() {
#if defined(_WIN32)
    if (const char* p = std::getenv("USERPROFILE")) return p;
#endif
    if (const char* p = std::getenv("HOME")) return p;
    return fs::current_path();
}

}  // namespace

fs::path configDir() {
    if (const char* p = std::getenv("SPROUT_EDITOR_CONFIG_DIR")) return p;
#if defined(_WIN32)
    if (const char* p = std::getenv("APPDATA")) return fs::path(p) / "SproutEditor";
    return home() / "SproutEditor";
#elif defined(__APPLE__)
    return home() / "Library" / "Application Support" / "SproutEditor";
#else
    if (const char* p = std::getenv("XDG_CONFIG_HOME"); p && *p) return fs::path(p) / "sprout-editor";
    return home() / ".config" / "sprout-editor";
#endif
}

namespace {

fs::path absoluteOf(const fs::path& p) {
    std::error_code ec;
    fs::path a = fs::weakly_canonical(fs::absolute(p, ec), ec);
    return ec ? p : a;
}

void save(const Recent& r) {
    json j;
    j["recent"] = json::array();
    for (auto& p : r.list) j["recent"].push_back(p.string());
    j["last"] = r.last.string();
    std::error_code ec;
    fs::create_directories(configDir(), ec);
    std::ofstream(configDir() / "projects.json") << j.dump(2) << '\n';
}

}  // namespace

Recent loadRecent() {
    Recent r;
    std::ifstream in(configDir() / "projects.json");
    if (!in) return r;
    try {
        json j = json::parse(in);
        for (auto& p : j.value("recent", json::array())) r.list.push_back(p.get<std::string>());
        r.last = j.value("last", std::string());
    } catch (const json::exception&) {
    }
    return r;
}

void remember(const fs::path& project) {
    fs::path p = absoluteOf(project);
    Recent r = loadRecent();
    r.list.erase(std::remove(r.list.begin(), r.list.end(), p), r.list.end());
    r.list.insert(r.list.begin(), p);
    if (r.list.size() > 30) r.list.resize(30);
    r.last = p;
    save(r);
}

void forget(const fs::path& project) {
    fs::path p = absoluteOf(project);
    Recent r = loadRecent();
    r.list.erase(std::remove(r.list.begin(), r.list.end(), p), r.list.end());
    if (r.last == p) r.last.clear();
    save(r);
}

bool isProject(const fs::path& dir) {
    std::error_code ec;
    return fs::is_regular_file(dir / "project.json", ec) || fs::is_directory(dir / "scenes", ec);
}

std::string projectName(const fs::path& dir) {
    std::error_code ec;
    if (fs::is_regular_file(dir / "project.json", ec)) return ProjectSettings::load(dir).name;
    return dir.filename().string();
}

bool createProject(const fs::path& dir, const std::string& name, std::string& error, const fs::path& templateDir) {
    std::error_code ec;
    if (fs::exists(dir, ec) && !fs::is_empty(dir, ec)) {
        error = dir.string() + " already exists and isn't empty.";
        return false;
    }
    if (!templateDir.empty()) {
        // A copy of the template, without what belongs to its own computer or builds
        fs::create_directories(dir, ec);
        for (fs::recursive_directory_iterator it(templateDir, ec), end; !ec && it != end; it.increment(ec)) {
            fs::path rel = fs::relative(it->path(), templateDir, ec);
            std::string top = rel.begin()->string();
            if (top == "exports" || top == ".vscode" || top == ".cache" || top == "compile_commands.json" ||
                top == "CppProperties.json" || top == "template.json") {
                if (it->is_directory()) it.disable_recursion_pending();
                continue;
            }
            if (it->is_directory()) fs::create_directories(dir / rel, ec);
            else fs::copy_file(it->path(), dir / rel, fs::copy_options::overwrite_existing, ec);
            if (ec) break;
        }
        if (ec) {
            error = "couldn't copy the template: " + ec.message();
            return false;
        }
        for (const char* sub : {"scenes", "assets", "scripts"}) fs::create_directories(dir / sub, ec);
        ProjectSettings settings = ProjectSettings::load(dir);
        settings.name = name.empty() ? dir.filename().string() : name;
        settings.exportDir.clear();
        if (!settings.save(dir)) {
            error = "can't write " + (dir / "project.json").string();
            return false;
        }
        if (!fs::exists(dir / ".gitignore")) std::ofstream(dir / ".gitignore") << "# Games exported from the editor\nexports/\n";
        return true;
    }
    for (const char* sub : {"scenes", "assets", "scripts"}) fs::create_directories(dir / sub, ec);
    if (ec) {
        error = "can't make " + dir.string() + ": " + ec.message();
        return false;
    }
    ProjectSettings settings;
    settings.name = name.empty() ? dir.filename().string() : name;
    if (!settings.save(dir)) {
        error = "can't write " + (dir / "project.json").string();
        return false;
    }
    // A first scene: an empty 2D world with a camera on the game window.
    json camera = {{"type", "Camera2D"},
                   {"name", "Camera2D"},
                   {"properties", {{"position", {settings.width / 2, settings.height / 2}}}},
                   {"children", json::array()}};
    json root = {{"type", "Node2D"}, {"name", "Main"}, {"properties", json::object()}, {"children", {camera}}};
    std::ofstream(dir / "scenes" / "main.scene.json")
        << json{{"format", "sprout-editor-scene"}, {"version", 1}, {"root", root}}.dump(2) << '\n';
    std::ofstream(dir / ".gitignore") << "# Games exported from the editor\nexports/\n";
    return true;
}

std::vector<ProjectTemplate> projectTemplates(const fs::path& editorDir) {
    std::vector<ProjectTemplate> out = {
        {"Empty", "A scene with a camera, and nothing else.", {}},
        {"2D Platformer", "The demo game: a player with animations, coins, a box and two levels.", editorDir / "project"},
    };
    std::error_code ec;
    std::vector<fs::path> folders;
    for (fs::directory_iterator it(editorDir / "templates", ec), end; !ec && it != end; it.increment(ec))
        if (it->is_directory()) folders.push_back(it->path());
    std::sort(folders.begin(), folders.end());
    for (const fs::path& f : folders) {
        ProjectTemplate t{f.filename().string(), "", f};
        try {
            std::ifstream in(f / "template.json");
            if (in) {
                json j = json::parse(in);
                t.name = j.value("name", t.name);
                t.description = j.value("description", "");
            }
        } catch (const json::exception&) {
        }
        out.push_back(t);
    }
    return out;
}

fs::path defaultProjectsFolder() { return home() / "SproutProjects"; }

fs::path workDir(const fs::path& project, const fs::path& buildDir) {
    fs::path p = absoluteOf(project);
    // Folder name plus a short hash of the full path, so two projects with the
    // same folder name don't share a build.
    std::string folder;
    for (char c : p.filename().string())
        folder += ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) ? c : '_';
    char hash[17];
    std::snprintf(hash, sizeof hash, "%08zx", std::hash<std::string>{}(p.generic_string()) & 0xffffffffu);
    return buildDir / "projects" / (folder + "-" + hash);
}

}  // namespace s2d::projects
