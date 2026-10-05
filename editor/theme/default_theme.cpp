// =============================================================================
//  The Sprout Editor's look. Change anything here, rebuild, restart the editor.
//
//  Colors are Color{red, green, blue, alpha}, each 0-255.
//  Sizes are in "UI pixels" (the editor's UI size setting scales them).
//  Pictures: put them in this folder (editor/theme/) and give the file name.
//
//  To use a different theme file instead of this one:
//      cmake -S . -B build -DSPROUT_EDITOR_THEME=editor/theme/light_theme.cpp
// =============================================================================
#include "raygui_include.hpp"  // for the ICON_... names and GuiSetStyle()
#include "theme.hpp"

namespace s2d {

Theme makeTheme() {
    Theme t;
    t.title = "Sprout Editor";

    // --- Widgets (buttons, text boxes, checkboxes, scroll bars) ---------------
    // One of raygui's looks: Default, Dark, Advance, Amber, Ashes, Bluish, Brick,
    // Candy, Cherry, Cyber, Enefete, Genesis, Jungle, Lavanda, Pocket, RLTech,
    // Sunny, Terminal, Turbo, Wisteria.
    t.widgets = WidgetStyle::Dark;
    t.customizeWidgets = [] {
        // Fine-tune any raygui setting, for example rounder looking buttons
        // with a thicker border:
        // GuiSetStyle(BUTTON, BORDER_WIDTH, 2);
        // GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, 0x64a0ffff);  // 0xRRGGBBAA
    };
    t.font = "";       // e.g. "MyFont.ttf" in this folder; "" = the built-in pixel font
    t.fontSize = 16;

    // --- Panels ---------------------------------------------------------------
    t.colors.background = Color{30, 32, 38, 255};
    t.colors.panel = Color{37, 40, 48, 255};
    t.colors.header = Color{26, 28, 34, 255};
    t.colors.border = Color{55, 60, 72, 255};
    t.colors.text = Color{220, 222, 228, 255};
    t.colors.dimText = Color{140, 146, 158, 255};
    t.colors.accent = Color{100, 160, 255, 255};
    t.colors.selection = Color{55, 85, 140, 255};
    t.colors.hover = Color{48, 53, 64, 255};
    t.colors.warning = Color{255, 160, 60, 255};
    t.colors.field = Color{24, 26, 31, 255};
    t.colors.good = Color{120, 220, 140, 255};
    t.colors.bad = Color{255, 110, 100, 255};
    t.colors.instancedText = Color{150, 175, 215, 255};
    t.colors.scriptPrint = Color{230, 230, 160, 255};
    t.colors.buildOutput = Color{120, 125, 135, 255};
    t.colors.checkerA = Color{58, 62, 72, 255};
    t.colors.checkerB = Color{46, 50, 58, 255};

    // --- Viewport -------------------------------------------------------------
    t.viewport.background = Color{22, 24, 29, 255};
    t.viewport.grid = Color{42, 45, 54, 255};
    t.viewport.xAxis = Color{170, 70, 70, 200};
    t.viewport.yAxis = Color{70, 170, 90, 200};
    t.viewport.gameFrame = Color{150, 110, 230, 200};
    t.viewport.selection = Color{255, 160, 60, 255};
    t.viewport.handleHot = Color{253, 249, 0, 255};
    t.viewport.playingBackground = Color{12, 12, 15, 255};
    t.viewport.playingBorder = Color{220, 90, 80, 255};
    t.viewport.vectorX = Color{220, 90, 90, 255};
    t.viewport.vectorY = Color{90, 200, 110, 255};

    t.gizmos.collisionShape = Color{60, 150, 255, 255};
    t.gizmos.collisionShapeDisabled = Color{150, 150, 150, 255};
    t.gizmos.camera = Color{190, 120, 255, 220};
    t.gizmos.missingTexture = Color{255, 0, 255, 255};

    // --- Node types -------------------------------------------------------------
    // {color, raygui icon, picture}. To use your own 16x16 picture instead of
    // an icon:  t.nodes["Sprite2D"] = {Color{130, 165, 255, 255}, 0, "sprite_icon.png"};
    const Color blue{130, 165, 255, 255}, physics{110, 200, 255, 255};
    t.nodes["Node"] = {Color{210, 210, 210, 255}, ICON_EMPTYBOX_SMALL, ""};
    t.nodes["Node2D"] = {blue, ICON_TARGET_SMALL, ""};
    t.nodes["Sprite2D"] = {blue, ICON_FILETYPE_IMAGE, ""};
    t.nodes["AnimatedSprite2D"] = {blue, ICON_FILETYPE_VIDEO, ""};
    t.nodes["ColorRect"] = {blue, ICON_BOX, ""};
    t.nodes["Label"] = {blue, ICON_TEXT_A, ""};
    t.nodes["Camera2D"] = {Color{190, 140, 255, 255}, ICON_EYE_ON, ""};
    t.nodes["CollisionShape2D"] = {physics, ICON_BOX_CORNERS_SMALL, ""};
    t.nodes["Area2D"] = {physics, ICON_BOX_DOTS_SMALL, ""};
    t.nodes["StaticBody2D"] = {physics, ICON_BOX_GRID, ""};
    t.nodes["CharacterBody2D"] = {physics, ICON_PLAYER, ""};
    t.otherNodes = {blue, ICON_EMPTYBOX_SMALL, ""};

    // --- Layout -------------------------------------------------------------------
    t.layout.toolbarHeight = 40;
    t.layout.headerHeight = 28;
    t.layout.rowHeight = 24;
    t.layout.leftWidth = 290;
    t.layout.rightWidth = 340;
    t.layout.bottomHeight = 170;
    t.layout.animationHeight = 280;
    t.layout.sceneDockShare = 0.55f;

    // --- Pictures -------------------------------------------------------------------
    t.images.windowIcon = "";
    t.images.toolbar = "";
    t.images.panels = "";        // e.g. a subtle pattern, tiled
    t.images.tilePanels = true;
    t.images.panelTint = Color{255, 255, 255, 40};
    t.images.viewport = "";
    t.images.viewportTint = Color{255, 255, 255, 255};
    return t;
}

}  // namespace s2d
