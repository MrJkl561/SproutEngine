// The editor's look: colors, fonts, images, node icons and panel sizes.
//
// Every value comes from a theme file in editor/theme/ (a .cpp with a
// makeTheme() function); default_theme.cpp is the one used unless CMake is told
// otherwise:  cmake -S . -B build -DSPROUT_EDITOR_THEME=editor/theme/light_theme.cpp
// Change a value, rebuild, restart the editor.
#pragma once

#include <raylib.h>

#include <functional>
#include <map>
#include <string>

namespace s2d {

// raygui's ready-made looks for buttons, text boxes, sliders...
enum class WidgetStyle {
    Default,  // raygui's own light grey
    Dark,
    Advance, Amber, Ashes, Bluish, Brick, Candy, Cherry, Cyber, Enefete,
    Genesis, Jungle, Lavanda, Pocket, RLTech, Sunny, Terminal, Turbo, Wisteria,
};

// How a node type shows in the Scene dock and the inspector.
struct NodeStyle {
    Color color{130, 165, 255, 255};
    int icon = 0;       // a raygui icon number (ICON_... in raygui.h)
    std::string image;  // or your own picture (drawn 16x16); "" = use icon
};

struct Theme {
    std::string title = "Sprout Editor";  // the window title starts with this

    // --- Widgets and text --------------------------------------------------
    WidgetStyle widgets = WidgetStyle::Dark;
    // Runs after the widget style is loaded: change anything with raygui's
    // GuiSetStyle(control, property, value) here.
    std::function<void()> customizeWidgets;
    std::string font;   // a .ttf/.otf file; "" = the built-in pixel font
    int fontSize = 16;  // in UI pixels (14-20 keeps the layout tidy)

    // --- Panel colors --------------------------------------------------------
    struct Colors {
        Color background{30, 32, 38, 255};    // behind everything
        Color panel{37, 40, 48, 255};         // dock and dialog backgrounds
        Color header{26, 28, 34, 255};        // panel title bars, the toolbar
        Color border{55, 60, 72, 255};
        Color text{220, 222, 228, 255};
        Color dimText{140, 146, 158, 255};    // labels, hints
        Color accent{100, 160, 255, 255};     // headings, highlights
        Color selection{55, 85, 140, 255};    // the selected row
        Color hover{48, 53, 64, 255};         // the row under the mouse
        Color warning{255, 160, 60, 255};     // unsaved, not compiled, changed values
        Color field{24, 26, 31, 255};         // number boxes
        Color good{120, 220, 140, 255};       // "saved", "ready"
        Color bad{255, 110, 100, 255};        // errors
        Color instancedText{150, 175, 215, 255};  // nodes that come from an instanced scene
        Color scriptPrint{230, 230, 160, 255};    // print() from scripts in the Output panel
        Color buildOutput{120, 125, 135, 255};    // compiler/export progress lines
        Color checkerA{58, 62, 72, 255}, checkerB{46, 50, 58, 255};  // behind see-through pictures
    } colors;

    // --- The 2D viewport -----------------------------------------------------
    struct Viewport {
        Color background{22, 24, 29, 255};
        Color grid{42, 45, 54, 255};
        Color xAxis{170, 70, 70, 200};
        Color yAxis{70, 170, 90, 200};
        Color gameFrame{150, 110, 230, 200};  // the game window's outline
        Color selection{255, 160, 60, 255};   // the selected node's outline and handles
        Color handleHot = YELLOW;             // the rotate handle under the mouse
        Color playingBackground{12, 12, 15, 255};
        Color playingBorder{220, 90, 80, 255};
        Color vectorX{220, 90, 90, 255};      // the little x / y marks on vector fields
        Color vectorY{90, 200, 110, 255};
    } viewport;

    // --- Editor-only drawings of nodes in the viewport ------------------------
    struct Gizmos {
        Color collisionShape{60, 150, 255, 255};
        Color collisionShapeDisabled{150, 150, 150, 255};
        Color camera{190, 120, 255, 220};
        Color missingTexture{255, 0, 255, 255};
    } gizmos;

    // --- The script editor (the Script tab) -------------------------------------
    struct Code {
        std::string font;   // a .ttf/.otf in the theme folder; "" = the UI font, drawn evenly spaced
        int fontSize = 0;   // 0 = the UI text size
        int tabSize = 4;    // spaces per indent
        Color background{24, 26, 31, 255};
        Color text{215, 218, 224, 255};
        Color lineNumbers{90, 96, 110, 255};
        Color currentLine{34, 37, 45, 255};
        Color selection{55, 85, 140, 200};
        Color cursor{235, 235, 240, 255};
        Color keyword{255, 120, 140, 255};     // if, return, class...
        Color type{120, 210, 200, 255};        // int, float, and names that start with a capital (Node2D)
        Color function{110, 175, 255, 255};    // name(...)
        Color string{255, 225, 140, 255};
        Color number{170, 230, 140, 255};
        Color comment{110, 118, 135, 255};
        Color preprocessor{220, 150, 255, 255};  // #include...
        Color key{150, 200, 255, 255};         // JSON keys
        // From clangd (live checking), when it's there:
        Color member{156, 210, 254, 255};      // a class's variables: velocity, position
        Color parameter{230, 200, 160, 255};   // a function's parameters: delta
        Color macro{220, 150, 255, 255};       // REGISTER_SCRIPT, KEY_SPACE
        Color constant{180, 220, 150, 255};    // enum values
        Color namespaceName{200, 200, 150, 255};  // std, s2d
        Color error{255, 90, 80, 255};         // compile errors (line marks and messages)
        Color warning{255, 180, 70, 255};
        Color match{90, 80, 30, 255};          // Find results
    } code;

    // --- Node types: color + icon (or image) -----------------------------------
    std::map<std::string, NodeStyle> nodes;  // by type name, e.g. "Sprite2D"
    NodeStyle otherNodes;                    // any type not in the map

    // --- Layout (UI pixels; the UI size setting scales all of it) -------------
    // These are the starting sizes: drag the borders between panels to change
    // them (remembered per user; double-click a border to go back to these).
    struct Layout {
        float toolbarHeight = 40;
        float headerHeight = 28;          // panel title bars
        float rowHeight = 24;             // Scene dock / FileSystem rows
        float leftWidth = 290;            // Scene + FileSystem docks
        float rightWidth = 340;           // Inspector
        float bottomHeight = 170;         // Output
        float animationHeight = 280;      // the bottom panel while the Animation tab shows
        float sceneDockShare = 0.55f;     // how much of the left side the Scene dock gets
    } layout;

    // --- Pictures (png, jpg...); "" = none -----------------------------------------
    // Relative paths are inside the theme file's folder.
    struct Images {
        std::string windowIcon;
        std::string toolbar;   // stretched across the toolbar
        std::string panels;    // behind every panel
        bool tilePanels = true;  // repeat the panel picture (else stretch it)
        Color panelTint{255, 255, 255, 40};  // how strongly it shows (alpha)
        std::string viewport;  // behind the viewport grid (stretched)
        Color viewportTint{255, 255, 255, 255};
    } images;
};

// Written in the theme file (editor/theme/default_theme.cpp unless changed).
Theme makeTheme();

// The theme in use (made once), and where its file's folder is.
const Theme& theme();
std::string themePath(const std::string& relative);  // "" stays ""

}  // namespace s2d
