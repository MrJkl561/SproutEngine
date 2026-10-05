// Includes raygui (https://github.com/raysan5/raygui) without its warnings.
// The implementation is compiled once, in raygui_impl.cpp.
#pragma once

#include <raylib.h>

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

#include <raygui.h>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace s2d {
void loadEditorStyle();      // the theme's widget style (see theme.hpp)
void moveTextCursorToEnd();
void setEditorFont(Font font);  // the font every control uses  // for a text box that just started editing
}
