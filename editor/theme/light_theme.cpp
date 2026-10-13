// A light theme: an example of a second theme file. Use it with
//     cmake -S . -B build -DSPROUT_EDITOR_THEME=editor/theme/light_theme.cpp
// Anything not set here keeps the default (see src/theme.hpp).
#include "raygui_include.hpp"
#include "theme.hpp"

namespace s2d {

Theme makeTheme() {
    Theme t;
    t.title = "Sprout Editor";
    t.widgets = WidgetStyle::Default;  // raygui's light look
    t.customizeWidgets = [] {
        GuiSetStyle(DEFAULT, BACKGROUND_COLOR, 0xf4f5f7ff);
        GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, 0xffffffff);
        GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, 0xc3c8d2ff);
        GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, 0x30343cff);
    };

    t.colors.background = Color{226, 229, 235, 255};
    t.colors.panel = Color{244, 245, 247, 255};
    t.colors.header = Color{218, 222, 229, 255};
    t.colors.border = Color{195, 200, 210, 255};
    t.colors.text = Color{36, 40, 48, 255};
    t.colors.dimText = Color{110, 116, 128, 255};
    t.colors.accent = Color{40, 110, 220, 255};
    t.colors.selection = Color{196, 216, 248, 255};
    t.colors.hover = Color{230, 234, 241, 255};
    t.colors.warning = Color{214, 120, 20, 255};
    t.colors.field = Color{255, 255, 255, 255};
    t.colors.good = Color{30, 150, 70, 255};
    t.colors.bad = Color{210, 60, 50, 255};
    t.colors.instancedText = Color{60, 100, 170, 255};
    t.colors.scriptPrint = Color{140, 110, 0, 255};
    t.colors.buildOutput = Color{95, 100, 112, 255};
    t.colors.checkerA = Color{225, 228, 233, 255};
    t.colors.checkerB = Color{205, 209, 216, 255};

    t.viewport.background = Color{200, 205, 213, 255};
    t.viewport.grid = Color{185, 190, 199, 255};
    t.viewport.xAxis = Color{200, 70, 70, 220};
    t.viewport.yAxis = Color{50, 150, 70, 220};
    t.viewport.gameFrame = Color{120, 80, 210, 220};
    t.viewport.selection = Color{230, 120, 20, 255};
    t.viewport.playingBackground = Color{60, 64, 72, 255};

    t.code.background = Color{255, 255, 255, 255};
    t.code.text = Color{36, 40, 48, 255};
    t.code.lineNumbers = Color{160, 166, 178, 255};
    t.code.currentLine = Color{242, 245, 250, 255};
    t.code.selection = Color{190, 212, 248, 255};
    t.code.cursor = Color{20, 20, 30, 255};
    t.code.keyword = Color{175, 30, 90, 255};
    t.code.type = Color{20, 120, 120, 255};
    t.code.function = Color{30, 90, 200, 255};
    t.code.string = Color{160, 90, 0, 255};
    t.code.number = Color{40, 130, 40, 255};
    t.code.comment = Color{130, 138, 150, 255};
    t.code.preprocessor = Color{130, 50, 180, 255};
    t.code.key = Color{30, 90, 200, 255};
    t.code.member = Color{0, 100, 150, 255};
    t.code.parameter = Color{140, 80, 20, 255};
    t.code.macro = Color{130, 50, 180, 255};
    t.code.constant = Color{30, 120, 60, 255};
    t.code.namespaceName = Color{110, 100, 30, 255};
    t.code.error = Color{210, 40, 30, 255};
    t.code.warning = Color{200, 120, 0, 255};
    t.code.match = Color{255, 236, 150, 255};

    const Color blue{40, 100, 200, 255}, physics{20, 130, 170, 255};
    t.nodes["Node"] = {Color{90, 96, 108, 255}, ICON_EMPTYBOX_SMALL, ""};
    t.nodes["Node2D"] = {blue, ICON_TARGET_SMALL, ""};
    t.nodes["Sprite2D"] = {blue, ICON_FILETYPE_IMAGE, ""};
    t.nodes["AnimatedSprite2D"] = {blue, ICON_FILETYPE_VIDEO, ""};
    t.nodes["ColorRect"] = {blue, ICON_BOX, ""};
    t.nodes["Label"] = {blue, ICON_TEXT_A, ""};
    t.nodes["Camera2D"] = {Color{130, 70, 200, 255}, ICON_EYE_ON, ""};
    t.nodes["CollisionShape2D"] = {physics, ICON_BOX_CORNERS_SMALL, ""};
    t.nodes["Area2D"] = {physics, ICON_BOX_DOTS_SMALL, ""};
    t.nodes["StaticBody2D"] = {physics, ICON_BOX_GRID, ""};
    t.nodes["CharacterBody2D"] = {physics, ICON_PLAYER, ""};
    t.nodes["TileMap"] = {Color{170, 100, 20, 255}, ICON_GRID, ""};
    const Color uiColor{30, 140, 60, 255};  // the UI nodes (green, like Godot's)
    t.nodes["CanvasLayer"] = {uiColor, ICON_LAYERS, ""};
    t.nodes["Panel"] = {uiColor, ICON_WINDOW, ""};
    t.nodes["Button"] = {uiColor, ICON_CURSOR_HAND, ""};
    t.nodes["CheckBox"] = {uiColor, ICON_SQUARE_TOGGLE, ""};
    t.nodes["HSlider"] = {uiColor, ICON_ARROW_RIGHT, ""};
    t.nodes["ProgressBar"] = {uiColor, ICON_LIFE_BARS, ""};
    t.nodes["TextureRect"] = {uiColor, ICON_FILETYPE_IMAGE, ""};
    t.nodes["LineEdit"] = {uiColor, ICON_TEXT_T, ""};
    t.nodes["VBoxContainer"] = {uiColor, ICON_VERTICAL_BARS, ""};
    t.nodes["HBoxContainer"] = {uiColor, ICON_BOX_GRID_BIG, ""};
    t.otherNodes = {blue, ICON_EMPTYBOX_SMALL, ""};
    return t;
}

}  // namespace s2d
