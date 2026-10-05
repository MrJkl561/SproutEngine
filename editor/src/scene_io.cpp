#include "scene_io.hpp"

#include <algorithm>
#include <fstream>
#include <functional>
#include <map>
#include <vector>

#include "assets.hpp"
#include "script.hpp"

namespace fs = std::filesystem;

namespace s2d {

using nlohmann::json;

json propertiesToJson(Node& node) {
    std::vector<Property> props;
    node.getProperties(props);
    return propertyListToJson(props);
}

json propertyListToJson(const std::vector<Property>& props) {
    json out = json::object();
    for (const Property& p : props) {
        switch (p.kind) {
            case PropKind::Section: break;
            case PropKind::Bool: out[p.name] = *static_cast<bool*>(p.ptr); break;
            case PropKind::Int: out[p.name] = *static_cast<int*>(p.ptr); break;
            case PropKind::Float: out[p.name] = *static_cast<float*>(p.ptr); break;
            case PropKind::Vec2: {
                auto* v = static_cast<Vector2*>(p.ptr);
                out[p.name] = {v->x, v->y};
                break;
            }
            case PropKind::Color: {
                auto* c = static_cast<Color*>(p.ptr);
                out[p.name] = {c->r, c->g, c->b, c->a};
                break;
            }
            case PropKind::String:
            case PropKind::Texture:
            case PropKind::Scene: out[p.name] = *static_cast<std::string*>(p.ptr); break;
        }
    }
    return out;
}

void applyProperties(Node& node, const json& in) {
    std::vector<Property> props;
    node.getProperties(props);
    applyPropertyList(props, in);
}

json scriptVarsToJson(const Node& node) {
    json out = json::object();
    for (auto& [name, text] : node.scriptVars) {
        try {
            out[name] = json::parse(text);
        } catch (const json::exception&) {
        }
    }
    return out;
}

void setScriptVars(Node& node, const json& values) {
    node.scriptVars.clear();
    if (!values.is_object()) return;
    for (auto& [name, value] : values.items()) node.scriptVars[name] = value.dump();
}

void applyScriptVars(Node& node) {
    if (!node.script || node.scriptVars.empty()) return;
    Exports e;
    node.script->exports(e);
    applyPropertyList(e.list, scriptVarsToJson(node));
}

void applyPropertyList(const std::vector<Property>& props, const json& in) {
    if (!in.is_object()) return;
    for (const Property& p : props) {
        if (p.kind == PropKind::Section || !in.contains(p.name)) continue;
        const json& v = in[p.name];
        try {
            switch (p.kind) {
                case PropKind::Section: break;
                case PropKind::Bool: *static_cast<bool*>(p.ptr) = v.get<bool>(); break;
                case PropKind::Int: *static_cast<int*>(p.ptr) = v.get<int>(); break;
                case PropKind::Float: *static_cast<float*>(p.ptr) = v.get<float>(); break;
                case PropKind::Vec2:
                    *static_cast<Vector2*>(p.ptr) = Vector2{v.at(0).get<float>(), v.at(1).get<float>()};
                    break;
                case PropKind::Color:
                    *static_cast<Color*>(p.ptr) =
                        Color{v.at(0).get<unsigned char>(), v.at(1).get<unsigned char>(),
                              v.at(2).get<unsigned char>(), v.at(3).get<unsigned char>()};
                    break;
                case PropKind::String:
                case PropKind::Texture:
                case PropKind::Scene: *static_cast<std::string*>(p.ptr) = v.get<std::string>(); break;
            }
        } catch (const json::exception&) {
            // A badly written value keeps its default.
        }
    }
}

std::string pathBetween(const Node* ancestor, const Node* node) {
    std::string path;
    for (const Node* n = node; n && n != ancestor; n = n->parent()) path = n->name + (path.empty() ? "" : "/" + path);
    return path;
}

namespace {

// The root node of a scene file as JSON, read once and kept until the file
// changes (instanced scenes are read every time a scene is saved or copied).
const json* sceneRootJson(const std::string& scenePath) {
    struct Entry {
        fs::file_time_type time;
        json root;
    };
    static std::map<std::string, Entry> cache;
    std::error_code ec;
    fs::path file = Assets::projectDir() / scenePath;
    // (Some file systems, like the one in web builds, may not know the time;
    // then the file is just read again each time.)
    auto time = fs::last_write_time(file, ec);
    auto it = cache.find(scenePath);
    if (!ec && it != cache.end() && it->second.time == time) return &it->second.root;
    std::ifstream in(file);
    if (!in) {
        cache.erase(scenePath);
        return nullptr;
    }
    try {
        json j = json::parse(in);
        Entry& e = cache[scenePath];
        e.time = time;
        e.root = j.at("root");
        return &e.root;
    } catch (const json::exception&) {
        cache.erase(scenePath);
        return nullptr;
    }
}

// Scene files being loaded right now, to catch a scene that instances itself.
std::vector<std::string>& loadingScenes() {
    static std::vector<std::string> stack;
    return stack;
}

std::unique_ptr<Node> instanceFromJson(const json& j) {
    std::string path = j.value("instance", std::string());
    auto& loading = loadingScenes();
    bool loop = std::find(loading.begin(), loading.end(), path) != loading.end();

    std::unique_ptr<Node> node;
    const json* source = loop ? nullptr : sceneRootJson(path);
    if (source) {
        loading.push_back(path);
        node = nodeFromJson(*source);
        loading.pop_back();
    }
    if (node) {
        markInstanceChildren(*node, true);
    } else {
        // Keep a stand-in, so the link (and its position etc.) isn't lost.
        print(loop ? "Scene '" + path + "' contains itself; it can't be instanced there."
                   : "Couldn't load the instanced scene '" + path + "' (moved or deleted?).");
        node = createNode(j.value("type", std::string("Node2D")));
        if (!node) node = createNode("Node2D");
        node->instanceMissing = true;
    }
    node->sceneFile = path;
    node->collapsed = true;  // an instance starts folded up in the Scene dock
    node->name = j.value("name", node->name);
    if (j.contains("script")) node->scriptName = j.value("script", std::string());
    if (j.contains("properties")) applyProperties(*node, j["properties"]);
    if (j.contains("script_vars")) setScriptVars(*node, j["script_vars"]);

    // Changes made to nodes inside this copy (they don't touch the original).
    if (j.contains("overrides") && j["overrides"].is_object() && !node->instanceMissing) {
        for (auto& [childPath, entry] : j["overrides"].items()) {
            Node* target = node->getNode(childPath);
            if (!target || target == node.get()) {
                print("'" + node->name + "': " + path + " no longer has a node at '" + childPath +
                      "', so a change made to it was dropped.");
                continue;
            }
            if (entry.contains("properties")) applyProperties(*target, entry["properties"]);
            if (entry.contains("script")) target->scriptName = entry.value("script", std::string());
            if (entry.contains("script_vars")) setScriptVars(*target, entry["script_vars"]);
        }
    }
    return node;
}

}  // namespace

void markInstanceChildren(Node& instanceRoot, bool inInstance) {
    for (auto& c : instanceRoot.children()) {
        c->inInstance = inInstance;
        if (c->sceneFile.empty()) markInstanceChildren(*c, inInstance);
    }
}

json nodeToJson(Node& node) {
    json j;
    j["type"] = node.typeName();
    j["name"] = node.name;

    if (!node.sceneFile.empty()) {
        // An instanced scene: a link, plus whatever differs from the file's root.
        j["instance"] = node.sceneFile;
        json mine = propertiesToJson(node);
        std::unique_ptr<Node> source = node.instanceMissing ? nullptr : instanceSource(node.sceneFile);
        if (source) {
            auto diff = [](Node& now, Node& original) {
                json changed = json::object();
                json theirs = propertiesToJson(original);
                json mineNow = propertiesToJson(now);  // (a named copy: items() doesn't keep a temporary alive)
                for (auto& [key, value] : mineNow.items())
                    if (!theirs.contains(key) || theirs[key] != value) changed[key] = value;
                return changed;
            };
            json changed = diff(node, *source);
            if (!changed.empty()) j["properties"] = changed;
            if (node.scriptName != source->scriptName) j["script"] = node.scriptName;
            if (node.scriptVars != source->scriptVars) j["script_vars"] = scriptVarsToJson(node);

            // Nodes inside the instance: save only what this copy changed.
            json overrides = json::object();
            std::function<void(Node&)> walk = [&](Node& n) {
                for (auto& c : n.children()) {
                    std::string childPath = pathBetween(&node, c.get());
                    if (Node* original = source->getNode(childPath); original && original != source.get()) {
                        json entry = json::object();
                        json props = diff(*c, *original);
                        if (!props.empty()) entry["properties"] = props;
                        if (c->scriptName != original->scriptName) entry["script"] = c->scriptName;
                        if (c->scriptVars != original->scriptVars) entry["script_vars"] = scriptVarsToJson(*c);
                        if (!entry.empty()) overrides[childPath] = entry;
                    }
                    walk(*c);
                }
            };
            walk(node);
            if (!overrides.empty()) j["overrides"] = overrides;
        } else {
            j["properties"] = mine;
            if (!node.scriptName.empty()) j["script"] = node.scriptName;
            if (!node.scriptVars.empty()) j["script_vars"] = scriptVarsToJson(node);
        }
        return j;
    }

    if (!node.scriptName.empty()) j["script"] = node.scriptName;
    if (!node.scriptVars.empty()) j["script_vars"] = scriptVarsToJson(node);
    j["properties"] = propertiesToJson(node);
    json& kids = j["children"] = json::array();
    for (auto& c : node.children()) kids.push_back(nodeToJson(*c));
    return j;
}

std::unique_ptr<Node> nodeFromJson(const json& j) {
    if (j.contains("instance")) return instanceFromJson(j);

    auto node = createNode(j.value("type", std::string("Node")));
    if (!node) return nullptr;
    node->name = j.value("name", std::string(node->typeName()));
    node->scriptName = j.value("script", std::string());
    if (j.contains("properties")) applyProperties(*node, j["properties"]);
    if (j.contains("script_vars")) setScriptVars(*node, j["script_vars"]);

    if (j.contains("children")) {
        for (const json& c : j["children"])
            if (auto child = nodeFromJson(c)) node->addChild(std::move(child));
    }
    return node;
}

std::unique_ptr<Node> cloneNode(Node& node) {
    auto copy = nodeFromJson(nodeToJson(node));
    if (copy && node.inInstance) copy->inInstance = true;
    return copy;
}

bool saveScene(Node& root, const fs::path& file, std::string* error) {
    // Build everything first, then write to a temporary file and swap it in,
    // so a problem part-way through can never leave a broken or empty scene.
    // The root of a scene file is always written out in full, even if it was
    // an instance where it came from ("Save Branch as Scene").
    std::string text;
    std::string link = root.sceneFile;
    try {
        root.sceneFile.clear();
        json j;
        j["format"] = "sprout-editor-scene";
        j["version"] = 1;
        j["root"] = nodeToJson(root);
        root.sceneFile = link;
        text = j.dump(2) + "\n";
    } catch (const std::exception& e) {
        root.sceneFile = link;
        if (error) *error = e.what();
        return false;
    }

    std::error_code ec;
    if (file.has_parent_path()) fs::create_directories(file.parent_path(), ec);
    fs::path temp = file;
    temp += ".saving";
    {
        std::ofstream out(temp, std::ios::binary);
        if (!out || !(out << text) || !out.flush()) {
            if (error) *error = "can't write " + temp.string();
            fs::remove(temp, ec);
            return false;
        }
    }
    fs::rename(temp, file, ec);  // replaces the old file
    if (ec) {
        if (error) *error = "can't replace " + file.string() + ": " + ec.message();
        fs::remove(temp, ec);
        return false;
    }
    return true;
}

std::unique_ptr<Node> loadScene(const fs::path& file, std::string* error) {
    std::ifstream in(file);
    if (!in) {
        if (error) *error = "can't open " + file.string();
        return nullptr;
    }
    try {
        json j = json::parse(in);
        std::error_code ec;
        std::string rel = fs::relative(file, Assets::projectDir(), ec).generic_string();
        auto& loading = loadingScenes();
        loading.push_back(rel);
        auto root = nodeFromJson(j.at("root"));
        loading.pop_back();
        if (!root && error) *error = "the scene's root node has an unknown type";
        return root;
    } catch (const json::exception& e) {
        loadingScenes().clear();
        if (error) *error = e.what();
        return nullptr;
    }
}

std::unique_ptr<Node> instanceSource(const std::string& scenePath) {
    json link;
    link["instance"] = scenePath;
    auto node = nodeFromJson(link);
    if (!node || node->instanceMissing) return nullptr;
    return node;
}

std::unique_ptr<Node> instantiate(const std::string& scenePath) {
    json link;
    link["instance"] = scenePath;
    auto node = nodeFromJson(link);
    if (!node || node->instanceMissing) return nullptr;
    // A spawned copy is just nodes; nothing links it back to the file.
    node->sceneFile.clear();
    markInstanceChildren(*node, false);
    return node;
}

bool sceneUses(const std::string& scenePath, const std::string& target) {
    if (scenePath == target) return true;
    std::vector<std::string> todo{scenePath}, seen;
    while (!todo.empty()) {
        std::string path = todo.back();
        todo.pop_back();
        if (std::find(seen.begin(), seen.end(), path) != seen.end()) continue;
        seen.push_back(path);
        const json* root = sceneRootJson(path);
        if (!root) continue;
        std::vector<const json*> nodes{root};
        while (!nodes.empty()) {
            const json* n = nodes.back();
            nodes.pop_back();
            if (n->contains("instance")) {
                std::string inner = n->value("instance", std::string());
                if (inner == target) return true;
                todo.push_back(inner);
            }
            if (n->contains("children"))
                for (const json& c : (*n)["children"]) nodes.push_back(&c);
        }
    }
    return false;
}

}  // namespace s2d
