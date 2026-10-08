#include "assets.hpp"

#include "multiplayer.hpp"

namespace s2d {

std::filesystem::path Assets::projectDir_;
std::map<std::string, std::unique_ptr<raylib::Texture>> Assets::textures_;

void Assets::setProjectDir(const std::filesystem::path& dir) { projectDir_ = dir; }
const std::filesystem::path& Assets::projectDir() { return projectDir_; }

raylib::Texture* Assets::texture(const std::string& path) {
    if (path.empty() || isHeadless()) return nullptr;  // a dedicated server has no window to draw in
    auto it = textures_.find(path);
    if (it == textures_.end()) {
        std::unique_ptr<raylib::Texture> tex;
        std::filesystem::path full = projectDir_ / path;
        if (std::filesystem::exists(full)) {
            try {
                tex = std::make_unique<raylib::Texture>(full.string());
                tex->SetFilter(TEXTURE_FILTER_POINT);
            } catch (const raylib::RaylibException&) {
                tex.reset();
            }
        }
        // Failed loads are cached as nullptr too, so we don't retry every frame.
        it = textures_.emplace(path, std::move(tex)).first;
    }
    return it->second.get();
}

void Assets::reload(const std::string& path) { textures_.erase(path); }

void Assets::clear() { textures_.clear(); }

}  // namespace s2d
