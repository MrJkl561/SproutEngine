// UI nodes (Godot's Control nodes): buttons, panels, sliders, text boxes...
//
//   CanvasLayer     - its children stay put on the screen: the camera doesn't
//                     move them. Put your HUD and menus inside one.
//   Panel           - a box (a color, or a picture stretched by nine-slicing)
//   Button          - click it: sends "pressed"
//   CheckBox        - on or off: sends "toggled(bool)"
//   HSlider         - drag to pick a number: sends "value_changed(float)"
//   ProgressBar     - shows a value as a bar (health, loading...)
//   TextureRect     - shows a picture, stretched or tiled to its size
//   LineEdit        - a box to type text in: "text_changed", "text_submitted"
//   VBoxContainer   - puts its children under each other
//   HBoxContainer   - puts its children side by side
//
// Every UI node has a position (its top-left corner) and a size. Connect their
// signals in the Inspector, or in code:
//
//     connect(getNode("Menu/Start"), "pressed", [this] { changeScene("scenes/level1.scene.json"); });
//     getNode("Hud/Health")->as<ProgressBar>()->value = health;
#pragma once

#include <string>

#include "nodes.hpp"

namespace s2d {

// Its children are drawn on top of the game, at fixed places on the screen
// (0,0 is the game window's top-left corner), whatever the camera does.
class CanvasLayer : public Node2D {
public:
    int layer = 1;  // higher layers are drawn on top of lower ones

    const char* typeName() const override { return "CanvasLayer"; }
    void getProperties(std::vector<Property>& out) override;
};

// The base of the UI nodes: a rectangle from position to position + size.
class Control : public Node2D {
public:
    raylib::Vector2 size{100, 32};
    bool disabled = false;  // greyed out and doesn't react to the mouse

    void getProperties(std::vector<Property>& out) override;
    Rectangle localRect() const override { return {0, 0, size.x, size.y}; }
    void getSignals(std::vector<SignalInfo>& out) const override;  // mouse_entered, mouse_exited

    bool isHovered() const { return hovered_; }  // the mouse is over it (and nothing on top)
    bool hasFocus() const { return focused_; }    // it gets the keyboard (a LineEdit being typed in)

    // --- Used by the engine ---------------------------------------------------
    // What the mouse is doing to this control this frame (worked out by the SceneTree).
    struct Input {
        raylib::Vector2 mouse;  // the mouse, in this control's own space
        bool hovered = false;   // under the mouse, on top
        bool pressed = false;   // the left button went down on it this frame
        bool held = false;      // it has the mouse: pressed on it and not yet let go
        bool released = false;  // the left button came up after pressing it this frame
        bool focused = false;
    };
    virtual void guiInput(const Input& in);
    virtual bool takesMouse() const { return true; }     // blocks clicks to whatever is below
    virtual bool takesFocus() const { return false; }    // wants the keyboard when clicked
    void setFocused(bool f) { focused_ = f; }

protected:
    // Its rectangle on the screen (scale applies, rotation doesn't).
    Rectangle screenRect() const;
    float scaleY() const;

private:
    bool hovered_ = false, focused_ = false;
};

// How a Panel or Button looks: a color with an optional border and rounded
// corners, or a picture (tinted by the color) stretched with nine-slicing:
// the corners keep their size, the edges and the middle stretch.
struct BoxStyle {
    raylib::Color color{50, 56, 66, 230};
    raylib::Color borderColor{110, 120, 135, 255};
    float borderWidth = 0;
    float cornerRadius = 4;
    std::string texture;     // e.g. "assets/ui/panel.png"
    int patchMargin = 8;     // the size of the picture's corners, in pixels

    void draw(Rectangle r, Color tint) const;
    void addProperties(std::vector<Property>& out, const std::string& prefix = "");
};

class Panel : public Control {
public:
    BoxStyle style;

    Panel() { size = raylib::Vector2(200, 120); }
    const char* typeName() const override { return "Panel"; }
    void getProperties(std::vector<Property>& out) override;
    void draw(const DrawOptions&) override;
};

class Button : public Control {
public:
    std::string text = "Button";
    int fontSize = 20;
    raylib::Color textColor = WHITE;
    BoxStyle style;
    raylib::Color hoverColor{80, 90, 105, 240};
    raylib::Color pressedColor{35, 40, 48, 240};
    bool toggleMode = false;     // stays down when clicked, until clicked again
    bool buttonPressed = false;  // (toggle mode) it's down

    Button();
    const char* typeName() const override { return "Button"; }
    void getProperties(std::vector<Property>& out) override;
    // pressed, button_down, button_up, toggled(bool)
    void getSignals(std::vector<SignalInfo>& out) const override;
    void draw(const DrawOptions&) override;
    void guiInput(const Input& in) override;
    void press();  // acts as if clicked (sends "pressed", flips a toggle button)

private:
    bool down_ = false;  // held down by the mouse right now
};

class CheckBox : public Control {
public:
    std::string text = "CheckBox";
    bool checked = false;
    int fontSize = 20;
    raylib::Color textColor = WHITE;
    raylib::Color boxColor{50, 56, 66, 255};
    raylib::Color checkColor{120, 200, 130, 255};

    CheckBox() { size = raylib::Vector2(160, 28); }
    const char* typeName() const override { return "CheckBox"; }
    void getProperties(std::vector<Property>& out) override;
    void getSignals(std::vector<SignalInfo>& out) const override;  // toggled(bool)
    void draw(const DrawOptions&) override;
    void guiInput(const Input& in) override;
    void setChecked(bool on);  // sends "toggled" if it changes
};

class HSlider : public Control {
public:
    float minValue = 0, maxValue = 100, step = 1, value = 50;
    raylib::Color trackColor{50, 56, 66, 255};
    raylib::Color fillColor{90, 160, 230, 255};
    raylib::Color grabberColor{230, 235, 240, 255};

    HSlider() { size = raylib::Vector2(200, 24); }
    const char* typeName() const override { return "HSlider"; }
    void getProperties(std::vector<Property>& out) override;
    void getSignals(std::vector<SignalInfo>& out) const override;  // value_changed(float)
    void draw(const DrawOptions&) override;
    void guiInput(const Input& in) override;
    void setValue(float v);  // keeps it in range and on a step; sends "value_changed" if it changes
};

class ProgressBar : public Control {
public:
    float minValue = 0, maxValue = 100, value = 50;
    raylib::Color backgroundColor{40, 44, 52, 230};
    raylib::Color fillColor{110, 190, 120, 255};
    bool showPercentage = true;
    int fontSize = 16;
    raylib::Color textColor = WHITE;

    ProgressBar() { size = raylib::Vector2(200, 24); }
    const char* typeName() const override { return "ProgressBar"; }
    void getProperties(std::vector<Property>& out) override;
    void draw(const DrawOptions&) override;
    bool takesMouse() const override { return false; }
    float ratio() const;  // how full, 0 to 1
};

class TextureRect : public Control {
public:
    enum Stretch { Scale = 0, KeepAspect = 1, Tile = 2, KeepCentered = 3 };
    std::string texture;
    int stretchMode = Scale;  // 0 stretch to fill, 1 fit (keep its shape), 2 repeat, 3 actual size, centred
    raylib::Color modulate = WHITE;
    bool flipH = false, flipV = false;

    TextureRect() { size = raylib::Vector2(64, 64); }
    const char* typeName() const override { return "TextureRect"; }
    void getProperties(std::vector<Property>& out) override;
    void draw(const DrawOptions&) override;
    bool takesMouse() const override { return false; }
};

class LineEdit : public Control {
public:
    std::string text;
    std::string placeholder = "Type here...";
    int maxLength = 0;  // 0: no limit
    int fontSize = 20;
    raylib::Color textColor = WHITE;
    raylib::Color placeholderColor{150, 155, 165, 255};
    BoxStyle style;
    raylib::Color focusColor{90, 160, 230, 255};  // the border while typing

    LineEdit();
    const char* typeName() const override { return "LineEdit"; }
    void getProperties(std::vector<Property>& out) override;
    // text_changed(std::string text), text_submitted(std::string text) (Enter)
    void getSignals(std::vector<SignalInfo>& out) const override;
    void draw(const DrawOptions&) override;
    void guiInput(const Input& in) override;
    bool takesFocus() const override { return true; }
};

// Puts its children under each other (VBox) or side by side (HBox), in order,
// separation pixels apart, and grows to fit them.
class BoxContainer : public Control {
public:
    float separation = 4;
    bool fill = true;  // stretch the children across (VBox: to its width; HBox: to its height)

    void getProperties(std::vector<Property>& out) override;
    void update(float) override { layout(); }
    void draw(const DrawOptions& options) override;  // (lays out, so the editor shows it too)
    bool takesMouse() const override { return false; }
    void layout();

protected:
    virtual bool vertical() const = 0;
};

class VBoxContainer : public BoxContainer {
public:
    const char* typeName() const override { return "VBoxContainer"; }

protected:
    bool vertical() const override { return true; }
};

class HBoxContainer : public BoxContainer {
public:
    const char* typeName() const override { return "HBoxContainer"; }

protected:
    bool vertical() const override { return false; }
};

// --- Used by the engine -------------------------------------------------------
// Inside a CanvasLayer (so drawn on the screen, not in the world)?
CanvasLayer* canvasLayerOf(const Node* node);
// Hands the mouse and keyboard to the UI nodes for this frame (SceneTree::process).
class SceneTree;
void processGui(SceneTree& tree);

}  // namespace s2d
