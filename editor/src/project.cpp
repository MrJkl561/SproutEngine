#include "project.hpp"

#include <algorithm>
#include <fstream>

#include <nlohmann/json.hpp>

namespace s2d {

using nlohmann::json;

ProjectSettings ProjectSettings::load(const std::filesystem::path& projectDir) {
    ProjectSettings s;
    std::ifstream in(projectDir / "project.json");
    if (!in) return s;
    try {
        json j = json::parse(in);
        s.name = j.value("name", s.name);
        s.mainScene = j.value("main_scene", s.mainScene);
        s.width = std::clamp(j.value("width", s.width), 16, 8192);
        s.height = std::clamp(j.value("height", s.height), 16, 8192);
        s.exportDir = j.value("export_dir", s.exportDir);
        if (j.contains("export_targets") && j["export_targets"].is_array())
            s.exportTargets = j["export_targets"].get<std::vector<std::string>>();
        if (j.contains("run_instances") && j["run_instances"].is_object()) {
            const json& r = j["run_instances"];
            s.runInstances = std::clamp(r.value("count", 1), 1, kMaxInstances);
            if (r.contains("args") && r["args"].is_array()) s.instanceArgs = r["args"].get<std::vector<std::string>>();
        }
    } catch (const json::exception&) {
        // A broken file gives the defaults.
    }
    return s;
}

bool ProjectSettings::save(const std::filesystem::path& projectDir) const {
    json j;
    j["name"] = name;
    j["main_scene"] = mainScene;
    j["width"] = width;
    j["height"] = height;
    if (!exportDir.empty()) j["export_dir"] = exportDir;
    j["export_targets"] = exportTargets;
    bool anyArgs = std::any_of(instanceArgs.begin(), instanceArgs.end(), [](auto& a) { return !a.empty(); });
    if (runInstances > 1 || anyArgs) j["run_instances"] = {{"count", runInstances}, {"args", instanceArgs}};
    std::ofstream out(projectDir / "project.json");
    if (!out) return false;
    out << j.dump(2) << '\n';
    return static_cast<bool>(out);
}

}  // namespace s2d
