// Compiles raygui and its dark theme into the editor.
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

#include <raylib.h>
#define RAYGUI_IMPLEMENTATION
#include <raygui.h>
#include <styles/advance/style_advance.h>
#include <styles/amber/style_amber.h>
#include <styles/ashes/style_ashes.h>
#include <styles/bluish/style_bluish.h>
#include <styles/brick/style_brick.h>
#include <styles/candy/style_candy.h>
#include <styles/cherry/style_cherry.h>
#include <styles/cyber/style_cyber.h>
#include <styles/dark/style_dark.h>
#include <styles/enefete/style_enefete.h>
#include <styles/genesis/style_genesis.h>
#include <styles/jungle/style_jungle.h>
#include <styles/lavanda/style_lavanda.h>
#include <styles/pocket/style_pocket.h>
#include <styles/rltech/style_rltech.h>
#include <styles/sunny/style_sunny.h>
#include <styles/terminal/style_terminal.h>
#include <styles/turbo/style_turbo.h>
#include <styles/wisteria/style_wisteria.h>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "theme.hpp"

namespace s2d {

void loadEditorStyle() {
    // Only once: each style load also loads (and keeps) its own font.
    static bool loaded = false;
    if (loaded) return;
    loaded = true;
    const Theme& t = theme();
    switch (t.widgets) {
        case WidgetStyle::Default: GuiLoadStyleDefault(); break;
        case WidgetStyle::Dark: GuiLoadStyleDark(); break;
        case WidgetStyle::Advance: GuiLoadStyleAdvance(); break;
        case WidgetStyle::Amber: GuiLoadStyleAmber(); break;
        case WidgetStyle::Ashes: GuiLoadStyleAshes(); break;
        case WidgetStyle::Bluish: GuiLoadStyleBluish(); break;
        case WidgetStyle::Brick: GuiLoadStyleBrick(); break;
        case WidgetStyle::Candy: GuiLoadStyleCandy(); break;
        case WidgetStyle::Cherry: GuiLoadStyleCherry(); break;
        case WidgetStyle::Cyber: GuiLoadStyleCyber(); break;
        case WidgetStyle::Enefete: GuiLoadStyleEnefete(); break;
        case WidgetStyle::Genesis: GuiLoadStyleGenesis(); break;
        case WidgetStyle::Jungle: GuiLoadStyleJungle(); break;
        case WidgetStyle::Lavanda: GuiLoadStyleLavanda(); break;
        case WidgetStyle::Pocket: GuiLoadStylePocket(); break;
        case WidgetStyle::RLTech: GuiLoadStyleRLTech(); break;
        case WidgetStyle::Sunny: GuiLoadStyleSunny(); break;
        case WidgetStyle::Terminal: GuiLoadStyleTerminal(); break;
        case WidgetStyle::Turbo: GuiLoadStyleTurbo(); break;
        case WidgetStyle::Wisteria: GuiLoadStyleWisteria(); break;
    }
    GuiSetStyle(DEFAULT, TEXT_SIZE, t.fontSize);
    GuiSetStyle(DEFAULT, TEXT_SPACING, 1);
    if (t.customizeWidgets) t.customizeWidgets();
}

void setEditorFont(Font font) {
    GuiSetFont(font);
    guiIconFontOffsetY = 0;  // icons were baked into the old font; draw them as pixels instead
    GuiSetStyle(DEFAULT, TEXT_SIZE, theme().fontSize);
    GuiSetStyle(DEFAULT, TEXT_SPACING, 1);
}

// raygui keeps one cursor for every text box; it's clamped to the text length
// on the next draw, so a huge value puts it at the end.

void moveTextCursorToEnd() { textBoxCursorIndex = 1 << 20; }

}  // namespace s2d
