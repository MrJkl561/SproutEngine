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
#include <styles/dark/style_dark.h>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace s2d {

void loadEditorStyle() {
    GuiLoadStyleDark();
    GuiSetStyle(DEFAULT, TEXT_SIZE, 16);
    GuiSetStyle(DEFAULT, TEXT_SPACING, 1);
}

// raygui keeps one cursor for every text box; it's clamped to the text length
// on the next draw, so a huge value puts it at the end.
void moveTextCursorToEnd() { textBoxCursorIndex = 1 << 20; }

}  // namespace s2d
