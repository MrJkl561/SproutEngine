// Saving and loading scenes as JSON files (like Godot's .tscn files), and
// copying node trees (which is how the editor makes a fresh copy to play).
//
// A node with a sceneFile is an instanced scene: it's saved as a link to that
// file plus only what differs from it ("overrides"), for the instance root and
// for any node inside it. Loading reads the file again and puts the overrides
// back on top. So the original scene is a template: changing it changes every
// copy, while changing a copy only changes that copy.
#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "nodes.hpp"

namespace s2d {

nlohmann::json nodeToJson(Node& node);
std::unique_ptr<Node> nodeFromJson(const nlohmann::json& j);  // nullptr on unknown types
std::unique_ptr<Node> cloneNode(Node& node);

// A node's properties as JSON ({"position": [x, y], ...}), and back.
nlohmann::json propertiesToJson(Node& node);
void applyProperties(Node& node, const nlohmann::json& properties);
// The same for any list of properties (e.g. a script's Exports).
nlohmann::json propertyListToJson(const std::vector<Property>& props);
void applyPropertyList(const std::vector<Property>& props, const nlohmann::json& values);

// A node's script variables (Node::scriptVars) as one JSON object, and back.
nlohmann::json scriptVarsToJson(const Node& node);
void setScriptVars(Node& node, const nlohmann::json& values);
// Puts the node's saved script variables into its script (before ready()).
void applyScriptVars(Node& node);

// The scene file as it is, with no overrides: what an instance would look like
// if nothing in it had been changed. nullptr if it can't be loaded.
std::unique_ptr<Node> instanceSource(const std::string& scenePath);

// "Sprite2D" or "Body/Sprite2D": the path from `ancestor` down to `node`.
std::string pathBetween(const Node* ancestor, const Node* node);

bool saveScene(Node& root, const std::filesystem::path& file, std::string* error = nullptr);
std::unique_ptr<Node> loadScene(const std::filesystem::path& file, std::string* error = nullptr);

// Makes a new copy of a scene file, e.g. instantiate("scenes/coin.scene.json").
// The path is inside the project folder. Returns nullptr (and prints why) if it
// can't be loaded. Scripts can use this to spawn things while the game runs.
std::unique_ptr<Node> instantiate(const std::string& scenePath);

// True if the scene file (a path inside the project) is `target` or instances
// it somewhere inside, at any depth. Used to stop a scene containing itself.
bool sceneUses(const std::string& scenePath, const std::string& target);

// Marks everything below an instance root as part of the instance (or, with
// false, makes it editable again). Nested instances keep their own children.
void markInstanceChildren(Node& instanceRoot, bool inInstance);

}  // namespace s2d
