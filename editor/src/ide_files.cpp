#include "ide_files.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;
using nlohmann::json;

namespace s2d {
namespace {

const char* kGitignoreMark = "# Code editor setup made by the Sprout Editor (paths on this computer)";

bool readText(const fs::path& file, std::string& text) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    text = ss.str();
    return true;
}

// Writes only when the text changed, so editors watching the file aren't disturbed.
bool writeIfChanged(const fs::path& file, const std::string& text, std::string& error) {
    std::string old;
    if (readText(file, old) && old == text) return true;
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::binary);
    if (!(out << text)) {
        error += (error.empty() ? "" : "\n") + std::string("Couldn't write ") + file.string();
        return false;
    }
    return true;
}

// Reads a JSON file of the user's. Returns false if it's there but can't be
// changed safely (not valid JSON, or has comments that writing would lose).
bool readUserJson(const fs::path& file, json& out) {
    std::string text;
    if (!readText(file, text)) {
        out = json::object();
        return true;
    }
    out = json::parse(text, nullptr, false);
    return !out.is_discarded() && out.is_object();
}

// The "Sprout" entry of a {"configurations": [...]} file, made if missing.
json& sproutConfig(json& file) {
    if (!file.contains("configurations") || !file["configurations"].is_array())
        file["configurations"] = json::array();
    for (json& c : file["configurations"])
        if (c.is_object() && c.value("name", "") == "Sprout") return c;
    file["configurations"].push_back({{"name", "Sprout"}});
    return file["configurations"].back();
}

// Says why a file of the user's was left as it is.
void skipped(const fs::path& file, std::string& error) {
    if (!error.empty()) error += "\n";
    error += "Left " + file.string() +
             " as it is (it has comments or isn't plain JSON), so code editors may not find script.hpp. "
             "Delete it and reopen the project to have it made again.";
}

bool exists(const std::string& path) {
    std::error_code ec;
    return !path.empty() && fs::exists(fs::path(path), ec);
}

}  // namespace

bool writeIdeFiles(const IdeSetup& s, std::string& error) {
    const fs::path scripts = s.project / "scripts";
    const bool msvc = s.compilerId == "MSVC";
    std::vector<std::string> includes = {s.editorSrc.string(), (s.depsDir / "raylib-src" / "src").string(),
                                         (s.depsDir / "raylib_cpp-src" / "include").string(), scripts.string()};
    bool ok = true;

    // compile_commands.json: one entry per script. clangd guesses the settings
    // for files that aren't listed (a new script) from the others.
    std::vector<fs::path> files;
    std::error_code ec;
    for (fs::directory_iterator it(scripts, ec), end; !ec && it != end; it.increment(ec))
        if (it->path().extension() == ".cpp") files.push_back(it->path());
    std::sort(files.begin(), files.end());
    if (files.empty()) files.push_back(scripts / "script.cpp");
    json db = json::array();
    for (const fs::path& f : files) {
        json args = json::array({s.compiler.string()});
        if (msvc) {
            for (const char* a : {"/std:c++20", "/EHsc", "/utf-8", "/permissive-"}) args.push_back(a);
            for (auto& i : includes) args.push_back("/I" + i);
        } else {
            args.push_back("-std=c++20");
            for (auto& i : includes) args.push_back("-I" + i);
        }
        args.push_back("-c");
        args.push_back(f.string());
        db.push_back({{"directory", scripts.string()}, {"file", f.string()}, {"arguments", args}});
    }
    ok &= writeIfChanged(s.project / "compile_commands.json", db.dump(2) + "\n", error);

    // VS Code's C/C++ extension (IntelliSense)
    {
        fs::path file = s.project / ".vscode" / "c_cpp_properties.json";
        json props;
        if (readUserJson(file, props)) {
            json& c = sproutConfig(props);
            c["includePath"] = includes;
            c["defines"] = json::array();
            c["cppStandard"] = "c++20";
            c["compilerPath"] = s.compiler.string();
            c["compileCommands"] = "${workspaceFolder}/compile_commands.json";
            if (!props.contains("version")) props["version"] = 4;
            ok &= writeIfChanged(file, props.dump(4) + "\n", error);
        } else {
            skipped(file, error);
            ok = false;
        }
    }

    // VS Code's clangd extension: use the clangd that came with the editor, so
    // there's nothing to install. Settings of your own are left alone.
    {
        fs::path file = s.project / ".vscode" / "settings.json";
        json settings;
        if (readUserJson(file, settings)) {
            json before = settings;
            if (exists(s.clangd.string()) && !exists(settings.value("clangd.path", "")))
                settings["clangd.path"] = s.clangd.string();
            // Where g++/clang++ keep the standard headers
            if (!msvc && !s.compiler.empty()) {
                std::string query = "--query-driver=" + s.compiler.string();
                auto& a = settings["clangd.arguments"];
                bool ours = a.is_null() || (a.is_array() && a.size() == 1 && a[0].is_string() &&
                                            a[0].get<std::string>().rfind("--query-driver=", 0) == 0);
                if (ours) a = json::array({query});
            }
            if (settings != before) ok &= writeIfChanged(file, settings.dump(4) + "\n", error);
        } else if (exists(s.clangd.string())) {
            skipped(file, error);
            ok = false;
        }
    }

#ifdef _WIN32
    // Visual Studio: File > Open > Folder
    {
        fs::path file = s.project / "CppProperties.json";
        json props;
        if (readUserJson(file, props)) {
#if defined(_M_ARM64)
            const char *env = "msvc_arm64", *mode = "windows-msvc-arm64";
#else
            const char *env = "msvc_x64", *mode = "windows-msvc-x64";
#endif
            json& c = sproutConfig(props);
            json paths = includes;
            paths.push_back("${env.INCLUDE}");
            c["inheritEnvironments"] = json::array({env});
            c["includePath"] = paths;
            c["defines"] = json::array();
            c["compilerSwitches"] = "/std:c++20 /EHsc /utf-8 /permissive-";
            c["intelliSenseMode"] = mode;
            ok &= writeIfChanged(file, props.dump(4) + "\n", error);
        } else {
            skipped(file, error);
            ok = false;
        }
    }
#endif

    // Keep them out of git: they only work on this computer.
    {
        fs::path file = s.project / ".gitignore";
        std::string text;
        readText(file, text);
        if (text.find(kGitignoreMark) == std::string::npos) {
            if (!text.empty() && text.back() != '\n') text += '\n';
            if (!text.empty()) text += '\n';
            text += std::string(kGitignoreMark) + "\ncompile_commands.json\n.vscode/c_cpp_properties.json\n"
                    ".vscode/settings.json\nCppProperties.json\n.cache/\n";
            ok &= writeIfChanged(file, text, error);
        }
    }
    return ok;
}

}  // namespace s2d
