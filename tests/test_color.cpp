// Tests for sprout/color.hpp.
#include <doctest/doctest.h>

#include <sprout/color.hpp>

#include "test_helpers.hpp"

using namespace sprout;

// The ready-made colors are constexpr, so they can be used at compile time.
static_assert(Color::Red.r == 230 && Color::Red.g == 41 && Color::Red.b == 55 && Color::Red.a == 255);
static_assert(Color::White == Color{255, 255, 255, 255});
static_assert(Color::Black == Color{0, 0, 0});
static_assert(Color::Blank.a == 0);
static_assert(Color::Transparent.a == 0);
static_assert(Color::fromHex(0xFF8800) == Color{255, 136, 0});
static_assert(Color{300, -5, 128}.r == 255, "out-of-range values are clamped at compile time too");
namespace {
constexpr Color kPaletteCopy = Color::SkyBlue; // usable to initialise other constexpr values
static_assert(kPaletteCopy.b == 255);
} // namespace

TEST_CASE("Color constructors") {
    SUBCASE("default color is solid white") {
        const Color c;
        CHECK(c == Color{255, 255, 255, 255});
    }
    SUBCASE("red, green, blue with alpha defaulting to 255") {
        const Color c{10, 20, 30};
        CHECK(c.r == 10);
        CHECK(c.g == 20);
        CHECK(c.b == 30);
        CHECK(c.a == 255);
    }
    SUBCASE("with an alpha value") {
        CHECK(Color{10, 20, 30, 40}.a == 40);
    }
    SUBCASE("from int variables (no narrowing errors)") {
        int r = 1, g = 2, b = 3, a = 4;
        const Color c{r, g, b, a};
        CHECK(c == Color{1, 2, 3, 4});
    }
    SUBCASE("from floats and doubles (rounded down)") {
        const Color c{127.9f, 0.5, 254.99f, 100.0};
        CHECK(c == Color{127, 0, 254, 100});
    }
    SUBCASE("values outside 0-255 are clamped instead of wrapping around") {
        CHECK(Color{300, 128, 0} == Color{255, 128, 0});
        CHECK(Color{-5, 128, 0} == Color{0, 128, 0});
        CHECK(Color{0, 0, 0, 1000} == Color{0, 0, 0, 255});
        CHECK(Color{0, 0, 0, -1} == Color{0, 0, 0, 0});
        CHECK(Color{256.5f, -0.5f, 255.0f} == Color{255, 0, 255});
    }
}

TEST_CASE("Color::withAlpha changes only the see-through amount") {
    const Color c = Color{10, 20, 30}.withAlpha(0.5f);
    CHECK(c.r == 10);
    CHECK(c.g == 20);
    CHECK(c.b == 30);
    CHECK(c.a == 127);
    CHECK(Color::Red.withAlpha(0.0f).a == 0);
    CHECK(Color::Red.withAlpha(1.0f).a == 255);
    CHECK(Color::Red.withAlpha(2.0f).a == 255); // clamped
    CHECK(Color::Red.withAlpha(-1.0f).a == 0);  // clamped
}

TEST_CASE("Color::lighter and Color::darker") {
    const Color base{100, 100, 100, 200};

    CHECK(base.lighter(0.0f) == base);
    CHECK(base.darker(0.0f) == base);
    CHECK(base.lighter(1.0f) == Color{255, 255, 255, 200}); // keeps alpha
    CHECK(base.darker(1.0f) == Color{0, 0, 0, 200});
    CHECK(base.lighter(0.5f) == Color{177, 177, 177, 200});
    CHECK(base.darker(0.5f) == Color{50, 50, 50, 200});

    SUBCASE("default amount is a small step") {
        CHECK(base.lighter().r > base.r);
        CHECK(base.lighter().r < 255);
        CHECK(base.darker().r < base.r);
        CHECK(base.darker().r > 0);
    }
}

TEST_CASE("Color::mix blends two colors") {
    const Color a{0, 0, 0, 0};
    const Color b{200, 100, 50, 255};
    CHECK(Color::mix(a, b, 0.0f) == a);
    CHECK(Color::mix(a, b, 1.0f) == b);
    CHECK(Color::mix(a, b, 0.5f) == Color{100, 50, 25, 127});
    CHECK(Color::mix(b, a, 0.5f) == Color{100, 50, 25, 127});
    // Blending "backwards" (to a smaller value) works too.
    CHECK(Color::mix(Color{255, 0, 0}, Color{0, 0, 255}, 0.25f) == Color{191, 0, 63});
}

TEST_CASE("Color::fromHex reads website-style colors") {
    CHECK(Color::fromHex(0xFF8800) == Color{255, 136, 0, 255});
    CHECK(Color::fromHex(0x000000) == Color{0, 0, 0});
    CHECK(Color::fromHex(0xFFFFFF) == Color{255, 255, 255});
    CHECK(Color::fromHex(0x123456) == Color{0x12, 0x34, 0x56});
}

TEST_CASE("Color::fromHSV goes around the rainbow") {
    CHECK(Color::fromHSV(0) == Color{255, 0, 0});
    CHECK(Color::fromHSV(60) == Color{255, 255, 0});
    CHECK(Color::fromHSV(120) == Color{0, 255, 0});
    CHECK(Color::fromHSV(180) == Color{0, 255, 255});
    CHECK(Color::fromHSV(240) == Color{0, 0, 255});
    CHECK(Color::fromHSV(300) == Color{255, 0, 255});

    SUBCASE("hue wraps around at 360 and works with negative numbers") {
        CHECK(Color::fromHSV(360) == Color::fromHSV(0));
        CHECK(Color::fromHSV(480) == Color::fromHSV(120));
        CHECK(Color::fromHSV(-120) == Color::fromHSV(240));
        CHECK(Color::fromHSV(-360) == Color::fromHSV(0));
        CHECK(Color::fromHSV(720 + 240) == Color{0, 0, 255});
    }
    SUBCASE("saturation 0 gives gray, value 0 gives black") {
        CHECK(Color::fromHSV(200, 0.0f, 1.0f) == Color{255, 255, 255});
        CHECK(Color::fromHSV(200, 0.0f, 0.5f) == Color{127, 127, 127});
        CHECK(Color::fromHSV(200, 1.0f, 0.0f) == Color{0, 0, 0});
    }
    SUBCASE("saturation and value are clamped to 0-1") {
        CHECK(Color::fromHSV(0, 5.0f, 5.0f) == Color{255, 0, 0});
        CHECK(Color::fromHSV(0, -1.0f, -1.0f) == Color{0, 0, 0});
    }
    SUBCASE("always solid") {
        CHECK(Color::fromHSV(123, 0.4f, 0.7f).a == 255);
    }
}

TEST_CASE("Palette colors are distinct and solid") {
    const Color palette[] = {Color::White,  Color::LightGray, Color::Gray,     Color::DarkGray,  Color::Red,
                             Color::Maroon, Color::Pink,      Color::Orange,   Color::Gold,      Color::Yellow,
                             Color::Green,  Color::Lime,      Color::DarkGreen, Color::SkyBlue,  Color::Blue,
                             Color::DarkBlue, Color::Cyan,    Color::Purple,   Color::Violet,    Color::DarkPurple,
                             Color::Magenta, Color::Beige,    Color::Brown,    Color::DarkBrown, Color::OffWhite,
                             Color::Black};
    for (const Color& c : palette) CHECK(c.a == 255);
    for (std::size_t i = 0; i < std::size(palette); ++i)
        for (std::size_t j = i + 1; j < std::size(palette); ++j) CHECK(palette[i] != palette[j]);
}
