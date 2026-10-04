#include "scene_io.hpp"

#include <fstream>
#include <vector>

namespace s2d {

using nlohmann::json;

json nodeToJson(Node& node) {
    json j;
    j["type"] = node.typeName();
    j["name"] = node.name;
    if (!node.scriptName.empty()) j["script"] = node.scriptName;

    std::vector<Property> props;
    node.getProperties(props);
    json& out = j["properties"] = json::object();
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
            case PropKind::Texture: out[p.name] = *static_cast<std::string*>(p.ptr); break;
        }
    }

    json& kids = j["children"] = json::array();
    for (auto& c : node.children()) kids.push_back(nodeToJson(*c));
    return j;
}

std::unique_ptr<Node> nodeFromJson(const json& j) {
    auto node = createNode(j.value("type", std::string("Node")));
    if (!node) return nullptr;
    node->name = j.value("name", std::string(node->typeName()));
    node->scriptName = j.value("script", std::string());

    std::vector<Property> props;
    node->getProperties(props);
    const json empty = json::object();
    const json& in = j.contains("properties") ? j["properties"] : empty;
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
                case PropKind::Texture: *static_cast<std::string*>(p.ptr) = v.get<std::string>(); break;
            }
        } catch (const json::exception&) {
            // A badly written value keeps its default.
        }
    }

    if (j.contains("children")) {
        for (const json& c : j["children"])
            if (auto child = nodeFromJson(c)) node->addChild(std::move(child));
    }
    return node;
}

std::unique_ptr<Node> cloneNode(Node& node) { return nodeFromJson(nodeToJson(node)); }

bool saveScene(Node& root, const std::filesystem::path& file, std::string* error) {
    std::error_code ec;
    if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path(), ec);
    std::ofstream out(file);
    if (!out) {
        if (error) *error = "can't write " + file.string();
        return false;
    }
    json j;
    j["format"] = "sprout-editor-scene";
    j["version"] = 1;
    j["root"] = nodeToJson(root);
    out << j.dump(2) << '\n';
    return true;
}

std::unique_ptr<Node> loadScene(const std::filesystem::path& file, std::string* error) {
    std::ifstream in(file);
    if (!in) {
        if (error) *error = "can't open " + file.string();
        return nullptr;
    }
    try {
        json j = json::parse(in);
        auto root = nodeFromJson(j.at("root"));
        if (!root && error) *error = "the scene's root node has an unknown type";
        return root;
    } catch (const json::exception& e) {
        if (error) *error = e.what();
        return nullptr;
    }
}

}  // namespace s2d
