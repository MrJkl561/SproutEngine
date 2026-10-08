#include "theme.hpp"

#include <filesystem>

#include "editor_paths.hpp"

namespace s2d {

const Theme& theme() {
    static const Theme t = makeTheme();
    return t;
}

std::string themePath(const std::string& relative) {
    if (relative.empty()) return {};
    std::filesystem::path p = relative;
    if (p.is_absolute()) return p.string();
    return (editorPaths().themeDir / p).string();
}

}  // namespace s2d
