// Saving and loading scenes as JSON files (like Godot's .tscn files), and
// copying node trees (which is how the editor makes a fresh copy to play).
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

bool saveScene(Node& root, const std::filesystem::path& file, std::string* error = nullptr);
std::unique_ptr<Node> loadScene(const std::filesystem::path& file, std::string* error = nullptr);

}  // namespace s2d
