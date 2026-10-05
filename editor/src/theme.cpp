#include "theme.hpp"

#include <filesystem>

#ifndef SPROUT_EDITOR_THEME_DIR
#define SPROUT_EDITOR_THEME_DIR "."
#endif

namespace s2d {

const Theme& theme() {
    static const Theme t = makeTheme();
    return t;
}

std::string themePath(const std::string& relative) {
    if (relative.empty()) return {};
    std::filesystem::path p = relative;
    if (p.is_absolute()) return p.string();
    return (std::filesystem::path(SPROUT_EDITOR_THEME_DIR) / p).string();
}

}  // namespace s2d
