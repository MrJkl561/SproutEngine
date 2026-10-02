#include "sprout/color.hpp"

#include <cmath>

namespace sprout {

Color Color::fromHSV(float hue, float saturation, float value) {
    hue = std::fmod(hue, 360.0f);
    if (hue < 0.0f) hue += 360.0f;
    saturation = clamp(saturation, 0.0f, 1.0f);
    value = clamp(value, 0.0f, 1.0f);

    const float chroma = value * saturation;
    const float x = chroma * (1.0f - std::abs(std::fmod(hue / 60.0f, 2.0f) - 1.0f));
    const float m = value - chroma;

    float r = 0, g = 0, b = 0;
    if (hue < 60)       { r = chroma; g = x; }
    else if (hue < 120) { r = x; g = chroma; }
    else if (hue < 180) { g = chroma; b = x; }
    else if (hue < 240) { g = x; b = chroma; }
    else if (hue < 300) { r = x; b = chroma; }
    else                { r = chroma; b = x; }

    return Color{(r + m) * 255.0f, (g + m) * 255.0f, (b + m) * 255.0f};
}

} // namespace sprout
