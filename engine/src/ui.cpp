// The UI is made with raygui (https://github.com/raysan5/raygui), which draws
// with raylib. Our Rects are turned into raylib-cpp Rectangles with toRaylib().
#include "sprout/ui.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "internal.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wenum-compare"
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#if !defined(__clang__)
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif
#elif defined(_MSC_VER)
#pragma warning(push, 0)
#endif

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

// Ready-made looks from raygui's "styles" folder
#include <styles/candy/style_candy.h>
#include <styles/cyber/style_cyber.h>
#include <styles/dark/style_dark.h>
#include <styles/jungle/style_jungle.h>
#include <styles/sunny/style_sunny.h>
#include <styles/terminal/style_terminal.h>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace sprout {

namespace {

constexpr int kDefaultTextSize = 20;

// The size picked with setUITextSize() (0 = none), kept when the theme changes.
int gChosenTextSize = 0;

// Which text box is being typed in (identified by where it is on screen).
struct ActiveTextBox {
    bool active = false;
    Rect area;
};

ActiveTextBox& activeTextBox() {
    static ActiveTextBox box;
    return box;
}

} // namespace

namespace detail {
void initUI() {
    GuiLoadStyleDefault();
    GuiSetStyle(DEFAULT, TEXT_SIZE, kDefaultTextSize);
    GuiSetStyle(DEFAULT, TEXT_SPACING, 2);
}
} // namespace detail

bool button(const std::string& text, const Rect& area) { return GuiButton(toRaylib(area), text.c_str()) != 0; }

bool checkbox(const std::string& text, const Rect& area, bool& checked) {
    const bool before = checked;
    GuiCheckBox(toRaylib(area), text.c_str(), &checked);
    return checked != before;
}

bool slider(const std::string& text, const Rect& area, float& value, float min, float max) {
    const float before = value;
    // Show the label on the left and the current value on the right.
    const std::string valueText = std::to_string(value).substr(0, std::to_string(value).find('.') + 3);
    GuiSliderBar(toRaylib(area), text.c_str(), valueText.c_str(), &value, min, max);
    return value != before;
}

bool textBox(const Rect& area, std::string& text, int maxLength) {
    maxLength = std::clamp(maxLength, 1, 1024);
    std::vector<char> buffer(static_cast<std::size_t>(maxLength) + 1, '\0');
    text.copy(buffer.data(), static_cast<std::size_t>(maxLength));

    ActiveTextBox& active = activeTextBox();
    const bool editing = active.active && active.area == area;
    if (GuiTextBox(toRaylib(area), buffer.data(), maxLength + 1, editing)) {
        // raygui says "toggle typing mode" (clicked, or Enter pressed)
        if (editing) active.active = false;
        else active = ActiveTextBox{true, area};
    }
    const std::string updated = buffer.data();
    const bool changed = updated != text;
    text = updated;
    return changed;
}

void progressBar(const Rect& area, float value, float max) {
    float clamped = std::clamp(value, 0.0f, max);
    GuiProgressBar(toRaylib(area), nullptr, nullptr, &clamped, 0.0f, max);
}

void label(const std::string& text, const Rect& area) { GuiLabel(toRaylib(area), text.c_str()); }

void panel(const std::string& title, const Rect& area) { GuiPanel(toRaylib(area), title.c_str()); }

void setUITextSize(int size) {
    gChosenTextSize = std::max(6, size);
    GuiSetStyle(DEFAULT, TEXT_SIZE, gChosenTextSize);
}

void setUITheme(UITheme theme) {
    switch (theme) {
    case UITheme::Default: GuiLoadStyleDefault(); break;
    case UITheme::Dark: GuiLoadStyleDark(); break;
    case UITheme::Candy: GuiLoadStyleCandy(); break;
    case UITheme::Cyber: GuiLoadStyleCyber(); break;
    case UITheme::Jungle: GuiLoadStyleJungle(); break;
    case UITheme::Sunny: GuiLoadStyleSunny(); break;
    case UITheme::Terminal: GuiLoadStyleTerminal(); break;
    }
    if (theme == UITheme::Default) detail::initUI();
    // Themes bring their own font size; keep the player's choice if they made one.
    if (gChosenTextSize > 0) GuiSetStyle(DEFAULT, TEXT_SIZE, gChosenTextSize);
}

} // namespace sprout
