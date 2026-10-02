#pragma once
// =============================================================================
//  sprout/color.hpp - colors
//
//    Color::Red                     a ready-made color
//    Color{255, 128, 0}             red, green, blue (each 0 - 255)
//    Color{255, 128, 0, 100}        ...plus alpha: 0 = invisible, 255 = solid
//    Color::fromHex(0xFF8800)       like the colors on websites
//    Color::Red.withAlpha(0.5f)     half see-through red
// =============================================================================
#include "sprout/math.hpp"

namespace sprout {

struct Color {
    unsigned char r = 255;
    unsigned char g = 255;
    unsigned char b = 255;
    unsigned char a = 255; ///< alpha: 0 = invisible, 255 = solid

    constexpr Color() = default;

    /// Color{red, green, blue} or Color{red, green, blue, alpha}. Each is 0 - 255.
    template <detail::Number R, detail::Number G, detail::Number B, detail::Number A = int>
    constexpr Color(R red, G green, B blue, A alpha = 255)
        : r(toByte(red)), g(toByte(green)), b(toByte(blue)), a(toByte(alpha)) {}

    /// A copy of this color with a different see-through amount (0.0 = invisible, 1.0 = solid).
    constexpr Color withAlpha(float alpha) const {
        return Color{r, g, b, clamp(alpha, 0.0f, 1.0f) * 255.0f};
    }
    /// A lighter version. amount goes from 0.0 (no change) to 1.0 (white).
    constexpr Color lighter(float amount = 0.25f) const { return mix(*this, Color{255, 255, 255, a}, amount); }
    /// A darker version. amount goes from 0.0 (no change) to 1.0 (black).
    constexpr Color darker(float amount = 0.25f) const { return mix(*this, Color{0, 0, 0, a}, amount); }

    /// Blends two colors. t = 0 gives a, t = 1 gives b.
    static constexpr Color mix(Color from, Color to, float t) {
        return Color{from.r + (to.r - from.r) * t, from.g + (to.g - from.g) * t,
                     from.b + (to.b - from.b) * t, from.a + (to.a - from.a) * t};
    }
    /// A color from a hex number like 0xFF8800 (red = FF, green = 88, blue = 00).
    static constexpr Color fromHex(unsigned int rgb) {
        return Color{(rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF};
    }
    /// A color from hue (0 - 360, the rainbow), saturation (0 - 1) and brightness (0 - 1).
    /// Tip: Color::fromHSV(timeSinceStart() * 100, 1, 1) cycles through the rainbow!
    static Color fromHSV(float hue, float saturation = 1.0f, float value = 1.0f);

    constexpr bool operator==(const Color&) const = default;

    // Ready-made colors
    static const Color White, Black, Blank, Transparent;
    static const Color LightGray, Gray, DarkGray;
    static const Color Red, Maroon, Pink, Orange, Gold, Yellow;
    static const Color Green, Lime, DarkGreen;
    static const Color SkyBlue, Blue, DarkBlue, Cyan;
    static const Color Purple, Violet, DarkPurple, Magenta;
    static const Color Beige, Brown, DarkBrown, OffWhite;

private:
    template <class T>
    static constexpr unsigned char toByte(T value) {
        return static_cast<unsigned char>(clamp(static_cast<float>(value), 0.0f, 255.0f));
    }
};

inline constexpr Color Color::White{255, 255, 255};
inline constexpr Color Color::Black{0, 0, 0};
inline constexpr Color Color::Blank{0, 0, 0, 0};
inline constexpr Color Color::Transparent{0, 0, 0, 0};
inline constexpr Color Color::LightGray{200, 200, 200};
inline constexpr Color Color::Gray{130, 130, 130};
inline constexpr Color Color::DarkGray{80, 80, 80};
inline constexpr Color Color::Red{230, 41, 55};
inline constexpr Color Color::Maroon{190, 33, 55};
inline constexpr Color Color::Pink{255, 109, 194};
inline constexpr Color Color::Orange{255, 161, 0};
inline constexpr Color Color::Gold{255, 203, 0};
inline constexpr Color Color::Yellow{253, 249, 0};
inline constexpr Color Color::Green{0, 228, 48};
inline constexpr Color Color::Lime{0, 158, 47};
inline constexpr Color Color::DarkGreen{0, 117, 44};
inline constexpr Color Color::SkyBlue{102, 191, 255};
inline constexpr Color Color::Blue{0, 121, 241};
inline constexpr Color Color::DarkBlue{0, 82, 172};
inline constexpr Color Color::Cyan{0, 255, 255};
inline constexpr Color Color::Purple{200, 122, 255};
inline constexpr Color Color::Violet{135, 60, 190};
inline constexpr Color Color::DarkPurple{112, 31, 126};
inline constexpr Color Color::Magenta{255, 0, 255};
inline constexpr Color Color::Beige{211, 176, 131};
inline constexpr Color Color::Brown{127, 106, 79};
inline constexpr Color Color::DarkBrown{76, 63, 47};
inline constexpr Color Color::OffWhite{245, 245, 245};

} // namespace sprout
