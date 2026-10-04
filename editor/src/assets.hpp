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
    // (Defined in assets.cpp, not here, so a hot-reloaded scripts library
    // uses the editor's copy instead of getting an empty one of its own.)
    static void setProjectDir(const std::filesystem::path& dir);
    static const std::filesystem::path& projectDir();

    // "assets/icon.png" -> the loaded texture, or nullptr if it can't be loaded.
    static raylib::Texture* texture(const std::string& path);
    static void reload(const std::string& path);  // forget a cached texture (file changed)
    static void clear();                          // unload everything (before the window closes)

private:
    static std::filesystem::path projectDir_;
    static std::map<std::string, std::unique_ptr<raylib::Texture>> textures_;
};

}  // namespace s2d
