#include "ui_scale.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#if !defined(__APPLE__)
#define GLFW_INCLUDE_NONE  // only GLFW itself, no OpenGL headers
#include <GLFW/glfw3.h>  // raylib is built on GLFW; ask it for the window's content scale
#endif

namespace s2d {

float systemUiScale() {
    float scale = 1.0f;
    if (const char* env = std::getenv("SPROUT_EDITOR_UI_SCALE")) {
        scale = std::strtof(env, nullptr);
    } else {
#if !defined(__APPLE__)
        if (GLFWwindow* window = glfwGetCurrentContext()) {
            float x = 1, y = 1;
            glfwGetWindowContentScale(window, &x, &y);
            scale = std::max(x, y);
        }
#endif
    }
    if (!(scale > 0)) scale = 1.0f;
    return std::clamp(std::round(scale * 4) / 4, 0.75f, 3.0f);
}

}  // namespace s2d
