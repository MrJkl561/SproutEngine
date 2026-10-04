// Loads textures from the project folder once and keeps them around.
#pragma once

#include <raylib-cpp.hpp>

#include <filesystem>
#include <map>
#include <memory>
#include <string>

namespace s2d {

class Assets {
public:
    static void setProjectDir(const std::filesystem::path& dir) { projectDir_ = dir; }
    static const std::filesystem::path& projectDir() { return projectDir_; }

    // "assets/icon.png" -> the loaded texture, or nullptr if it can't be loaded.
    static raylib::Texture* texture(const std::string& path);
    static void reload(const std::string& path);  // forget a cached texture (file changed)
    static void clear();                          // unload everything (before the window closes)

private:
    static inline std::filesystem::path projectDir_;
    static inline std::map<std::string, std::unique_ptr<raylib::Texture>> textures_;
};

}  // namespace s2d
