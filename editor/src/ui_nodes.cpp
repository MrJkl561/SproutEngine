#include "ui_nodes.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>

#include "assets.hpp"
#include "multiplayer.hpp"
#include "scene_tree.hpp"

namespace s2d {

namespace {

float fontSpacing(float size) { return size / 10; }

raylib::Vector2 measure(const std::string& text, float size) {
    return MeasureTextEx(GetFontDefault(), text.c_str(), size, fontSpacing(size));
}

void drawString(const std::string& text, raylib::Vector2 at, float size, Color color) {
    ::DrawTextEx(GetFontDefault(), text.c_str(), ::Vector2{at.x, at.y}, size, fontSpacing(size), color);
}

Color dimmed(Color c, bool disabled) { return disabled ? Fade(c, c.a / 255.0f * 0.45f) : c; }

}  // namespace

// ---------------------------------------------------------------------------
// CanvasLayer
// ---------------------------------------------------------------------------
void CanvasLayer::getProperties(std::vector<Property>& out) {
    Node2D::getProperties(out);
    out.push_back({"CanvasLayer", PropKind::Section});
    out.push_back({"layer", PropKind::Int, &layer, 0.05f});
}

CanvasLayer* canvasLayerOf(const Node* node) {
    for (const Node* n = node; n; n = n->parent())
        if (auto* layer = dynamic_cast<const CanvasLayer*>(n)) return const_cast<CanvasLayer*>(layer);
    return nullptr;
}

// ---------------------------------------------------------------------------
// Control
// ---------------------------------------------------------------------------
void Control::getProperties(std::vector<Property>& out) {
    Node2D::getProperties(out);
    out.push_back({"Control", PropKind::Section});
    out.push_back({"size", PropKind::Vec2, &size, 1.0f});
    out.push_back({"disabled", PropKind::Bool, &disabled});
}

void Control::getSignals(std::vector<SignalInfo>& out) const {
    Node2D::getSignals(out);
    out.push_back({"mouse_entered", ""});
    out.push_back({"mouse_exited", ""});
}

void Control::guiInput(const Input& in) {
    if (in.hovered != hovered_) {
        hovered_ = in.hovered;
        emitSignal(hovered_ ? "mouse_entered" : "mouse_exited");
    }
    focused_ = in.focused;
}

Rectangle Control::screenRect() const {
    Xform g = globalXform();
    float w = size.x * g.scale.x, h = size.y * g.scale.y;
    return {std::min(g.position.x, g.position.x + w), std::min(g.position.y, g.position.y + h), std::fabs(w),
            std::fabs(h)};
}

float Control::scaleY() const { return std::fabs(globalXform().scale.y); }

// ---------------------------------------------------------------------------
// BoxStyle
// ---------------------------------------------------------------------------
void BoxStyle::draw(Rectangle r, Color tint) const {
    if (r.width <= 0 || r.height <= 0) return;
    if (!texture.empty()) {
        if (raylib::Texture* tex = Assets::texture(texture)) {
            int m = std::clamp(patchMargin, 0, std::min(tex->width, tex->height) / 2);
            NPatchInfo info{{0, 0, static_cast<float>(tex->width), static_cast<float>(tex->height)}, m, m, m, m,
                            NPATCH_NINE_PATCH};
            DrawTextureNPatch(*tex, info, r, {0, 0}, 0, tint);
            return;
        }
    }
    float roundness = cornerRadius > 0 ? std::min(1.0f, cornerRadius * 2 / std::min(r.width, r.height)) : 0;
    if (roundness > 0) DrawRectangleRounded(r, roundness, 8, tint);
    else DrawRectangleRec(r, tint);
    if (borderWidth > 0) {
        if (roundness > 0) DrawRectangleRoundedLinesEx(r, roundness, 8, borderWidth, borderColor);
        else DrawRectangleLinesEx(r, borderWidth, borderColor);
    }
}

void BoxStyle::addProperties(std::vector<Property>& out, const std::string& prefix) {
    out.push_back({prefix + "color", PropKind::Color, &color});
    out.push_back({prefix + "border_color", PropKind::Color, &borderColor});
    out.push_back({prefix + "border_width", PropKind::Float, &borderWidth, 0.1f});
    out.push_back({prefix + "corner_radius", PropKind::Float, &cornerRadius, 0.1f});
    out.push_back({prefix + "texture", PropKind::Texture, &texture});
    out.push_back({prefix + "patch_margin", PropKind::Int, &patchMargin, 0.1f});
}

// ---------------------------------------------------------------------------
// Panel
// ---------------------------------------------------------------------------
void Panel::getProperties(std::vector<Property>& out) {
    Control::getProperties(out);
    out.push_back({"Panel", PropKind::Section});
    style.addProperties(out);
    addShaderProperties(out);
}

void Panel::draw(const DrawOptions&) { style.draw(screenRect(), style.color); }

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------
Button::Button() {
    size = raylib::Vector2(140, 40);
    style.color = raylib::Color(60, 68, 80, 240);
    style.borderWidth = 1;
}

void Button::getProperties(std::vector<Property>& out) {
    Control::getProperties(out);
    out.push_back({"Button", PropKind::Section});
    out.push_back({"text", PropKind::String, &text});
    out.push_back({"font_size", PropKind::Int, &fontSize, 0.2f});
    out.push_back({"text_color", PropKind::Color, &textColor});
    out.push_back({"toggle_mode", PropKind::Bool, &toggleMode});
    out.push_back({"button_pressed", PropKind::Bool, &buttonPressed});
    out.push_back({"Look", PropKind::Section});
    style.addProperties(out);
    out.push_back({"hover_color", PropKind::Color, &hoverColor});
    out.push_back({"pressed_color", PropKind::Color, &pressedColor});
    addShaderProperties(out);
}

void Button::getSignals(std::vector<SignalInfo>& out) const {
    Control::getSignals(out);
    out.push_back({"pressed", ""});          // clicked (the mouse went down and up on it)
    out.push_back({"button_down", ""});      // the mouse went down on it
    out.push_back({"button_up", ""});        // ...and came up again
    out.push_back({"toggled", "bool on"});   // toggle_mode: it went down or up
}

void Button::press() {
    if (toggleMode) {
        buttonPressed = !buttonPressed;
        emitSignal("toggled", buttonPressed);
    }
    emitSignal("pressed");
}

void Button::guiInput(const Input& in) {
    Control::guiInput(in);
    if (disabled) {
        down_ = false;
        return;
    }
    if (in.pressed) {
        down_ = true;
        emitSignal("button_down");
    }
    if (in.released && down_) {
        down_ = false;
        emitSignal("button_up");
        if (in.hovered) press();  // let go outside it: not a click
    }
    if (!in.held) down_ = false;
}

void Button::draw(const DrawOptions&) {
    Rectangle r = screenRect();
    bool down = (down_ && isHovered()) || (toggleMode && buttonPressed);
    Color fill = down ? pressedColor : isHovered() ? hoverColor : style.color;
    if (!style.texture.empty()) fill = down ? Color{200, 200, 200, 255} : isHovered() ? Color{235, 235, 235, 255} : WHITE;
    style.draw(r, dimmed(fill, disabled));
    float size = static_cast<float>(std::max(fontSize, 1)) * scaleY();
    raylib::Vector2 m = measure(text, size);
    float nudge = down ? 1.0f : 0.0f;
    drawString(text, {r.x + (r.width - m.x) / 2, r.y + (r.height - m.y) / 2 + nudge}, size, dimmed(textColor, disabled));
}

// ---------------------------------------------------------------------------
// CheckBox
// ---------------------------------------------------------------------------
void CheckBox::getProperties(std::vector<Property>& out) {
    Control::getProperties(out);
    out.push_back({"CheckBox", PropKind::Section});
    out.push_back({"text", PropKind::String, &text});
    out.push_back({"checked", PropKind::Bool, &checked});
    out.push_back({"font_size", PropKind::Int, &fontSize, 0.2f});
    out.push_back({"text_color", PropKind::Color, &textColor});
    out.push_back({"box_color", PropKind::Color, &boxColor});
    out.push_back({"check_color", PropKind::Color, &checkColor});
}

void CheckBox::getSignals(std::vector<SignalInfo>& out) const {
    Control::getSignals(out);
    out.push_back({"toggled", "bool on"});
}

void CheckBox::setChecked(bool on) {
    if (on == checked) return;
    checked = on;
    emitSignal("toggled", checked);
}

void CheckBox::guiInput(const Input& in) {
    Control::guiInput(in);
    if (!disabled && in.released && in.hovered) setChecked(!checked);
}

void CheckBox::draw(const DrawOptions&) {
    Rectangle r = screenRect();
    float box = std::min(r.height, 22 * scaleY());
    Rectangle b{r.x, r.y + (r.height - box) / 2, box, box};
    DrawRectangleRounded(b, 0.25f, 6, dimmed(boxColor, disabled));
    if (isHovered() && !disabled) DrawRectangleRoundedLinesEx(b, 0.25f, 6, 1, Fade(WHITE, 0.6f));
    if (checked) {
        float p = box * 0.22f;
        DrawRectangleRounded({b.x + p, b.y + p, box - 2 * p, box - 2 * p}, 0.25f, 6, dimmed(checkColor, disabled));
    }
    float size = static_cast<float>(std::max(fontSize, 1)) * scaleY();
    raylib::Vector2 m = measure(text, size);
    drawString(text, {b.x + box + 8 * scaleY(), r.y + (r.height - m.y) / 2}, size, dimmed(textColor, disabled));
}

// ---------------------------------------------------------------------------
// HSlider
// ---------------------------------------------------------------------------
void HSlider::getProperties(std::vector<Property>& out) {
    Control::getProperties(out);
    out.push_back({"HSlider", PropKind::Section});
    out.push_back({"min_value", PropKind::Float, &minValue, 0.1f});
    out.push_back({"max_value", PropKind::Float, &maxValue, 0.1f});
    out.push_back({"step", PropKind::Float, &step, 0.01f});
    out.push_back({"value", PropKind::Float, &value, 0.1f});
    out.push_back({"track_color", PropKind::Color, &trackColor});
    out.push_back({"fill_color", PropKind::Color, &fillColor});
    out.push_back({"grabber_color", PropKind::Color, &grabberColor});
}

void HSlider::getSignals(std::vector<SignalInfo>& out) const {
    Control::getSignals(out);
    out.push_back({"value_changed", "float value"});
}

void HSlider::setValue(float v) {
    float lo = std::min(minValue, maxValue), hi = std::max(minValue, maxValue);
    if (step > 0) v = minValue + std::round((v - minValue) / step) * step;
    v = std::clamp(v, lo, hi);
    if (v == value) return;
    value = v;
    emitSignal("value_changed", value);
}

void HSlider::guiInput(const Input& in) {
    Control::guiInput(in);
    if (disabled || !in.held || size.x <= 0) return;
    float t = std::clamp(in.mouse.x / size.x, 0.0f, 1.0f);
    setValue(minValue + t * (maxValue - minValue));
}

void HSlider::draw(const DrawOptions&) {
    Rectangle r = screenRect();
    float t = maxValue != minValue ? std::clamp((value - minValue) / (maxValue - minValue), 0.0f, 1.0f) : 0.0f;
    float track = std::max(4.0f, r.height * 0.25f);
    Rectangle tr{r.x, r.y + (r.height - track) / 2, r.width, track};
    DrawRectangleRounded(tr, 1, 6, dimmed(trackColor, disabled));
    DrawRectangleRounded({tr.x, tr.y, tr.width * t, track}, 1, 6, dimmed(fillColor, disabled));
    float radius = r.height * (isHovered() && !disabled ? 0.45f : 0.38f);
    DrawCircleV({r.x + r.width * t, r.y + r.height / 2}, radius, dimmed(grabberColor, disabled));
}

// ---------------------------------------------------------------------------
// ProgressBar
// ---------------------------------------------------------------------------
void ProgressBar::getProperties(std::vector<Property>& out) {
    Control::getProperties(out);
    out.push_back({"ProgressBar", PropKind::Section});
    out.push_back({"min_value", PropKind::Float, &minValue, 0.1f});
    out.push_back({"max_value", PropKind::Float, &maxValue, 0.1f});
    out.push_back({"value", PropKind::Float, &value, 0.1f});
    out.push_back({"show_percentage", PropKind::Bool, &showPercentage});
    out.push_back({"font_size", PropKind::Int, &fontSize, 0.2f});
    out.push_back({"background_color", PropKind::Color, &backgroundColor});
    out.push_back({"fill_color", PropKind::Color, &fillColor});
    out.push_back({"text_color", PropKind::Color, &textColor});
}

float ProgressBar::ratio() const {
    if (maxValue == minValue) return 0;
    return std::clamp((value - minValue) / (maxValue - minValue), 0.0f, 1.0f);
}

void ProgressBar::draw(const DrawOptions&) {
    Rectangle r = screenRect();
    float round = std::min(1.0f, 8 / std::max(1.0f, std::min(r.width, r.height)));
    DrawRectangleRounded(r, round, 6, backgroundColor);
    if (ratio() > 0) DrawRectangleRounded({r.x, r.y, r.width * ratio(), r.height}, round, 6, fillColor);
    if (showPercentage) {
        std::string pct = std::to_string(static_cast<int>(std::lround(ratio() * 100))) + "%";
        float size = static_cast<float>(std::max(fontSize, 1)) * scaleY();
        raylib::Vector2 m = measure(pct, size);
        drawString(pct, {r.x + (r.width - m.x) / 2, r.y + (r.height - m.y) / 2}, size, textColor);
    }
}

// ---------------------------------------------------------------------------
// TextureRect
// ---------------------------------------------------------------------------
void TextureRect::getProperties(std::vector<Property>& out) {
    Control::getProperties(out);
    out.push_back({"TextureRect", PropKind::Section});
    out.push_back({"texture", PropKind::Texture, &texture});
    out.push_back({"stretch_mode", PropKind::Int, &stretchMode, 0.05f});
    out.push_back({"modulate", PropKind::Color, &modulate});
    out.push_back({"flip_h", PropKind::Bool, &flipH});
    out.push_back({"flip_v", PropKind::Bool, &flipV});
    addShaderProperties(out);
}

void TextureRect::draw(const DrawOptions& options) {
    Rectangle r = screenRect();
    raylib::Texture* tex = Assets::texture(texture);
    if (!tex) {
        if (options.editor) DrawRectangleLinesEx(r, 1 / options.zoom, gizmoColors().missingTexture);
        return;
    }
    float tw = static_cast<float>(tex->width), th = static_cast<float>(tex->height);
    Rectangle src{0, 0, flipH ? -tw : tw, flipV ? -th : th};
    switch (std::clamp(stretchMode, 0, 3)) {
        case Scale: DrawTexturePro(*tex, src, r, {0, 0}, 0, modulate); break;
        case KeepAspect: {
            float s = std::min(r.width / tw, r.height / th);
            Rectangle d{r.x + (r.width - tw * s) / 2, r.y + (r.height - th * s) / 2, tw * s, th * s};
            DrawTexturePro(*tex, src, d, {0, 0}, 0, modulate);
            break;
        }
        case Tile: {
            float s = scaleY();
            float cw = tw * s, ch = th * s;
            if (cw < 1 || ch < 1) break;
            for (float y = 0; y < r.height; y += ch) {
                for (float x = 0; x < r.width; x += cw) {
                    float w = std::min(cw, r.width - x), h = std::min(ch, r.height - y);
                    Rectangle part{0, 0, w / s, h / s};  // a whole tile, or what fits at the edge
                    if (flipH) part.width = -part.width;
                    if (flipV) part.height = -part.height;
                    DrawTexturePro(*tex, part, {r.x + x, r.y + y, w, h}, {0, 0}, 0, modulate);
                }
            }
            break;
        }
        case KeepCentered: {
            float s = scaleY();
            Rectangle d{r.x + (r.width - tw * s) / 2, r.y + (r.height - th * s) / 2, tw * s, th * s};
            DrawTexturePro(*tex, src, d, {0, 0}, 0, modulate);
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// LineEdit
// ---------------------------------------------------------------------------
LineEdit::LineEdit() {
    size = raylib::Vector2(220, 36);
    style.color = raylib::Color(30, 34, 40, 240);
    style.borderWidth = 1;
}

void LineEdit::getProperties(std::vector<Property>& out) {
    Control::getProperties(out);
    out.push_back({"LineEdit", PropKind::Section});
    out.push_back({"text", PropKind::String, &text});
    out.push_back({"placeholder", PropKind::String, &placeholder});
    out.push_back({"max_length", PropKind::Int, &maxLength, 0.2f});
    out.push_back({"font_size", PropKind::Int, &fontSize, 0.2f});
    out.push_back({"text_color", PropKind::Color, &textColor});
    out.push_back({"placeholder_color", PropKind::Color, &placeholderColor});
    out.push_back({"Look", PropKind::Section});
    style.addProperties(out);
    out.push_back({"focus_color", PropKind::Color, &focusColor});
}

void LineEdit::getSignals(std::vector<SignalInfo>& out) const {
    Control::getSignals(out);
    out.push_back({"text_changed", "std::string text"});
    out.push_back({"text_submitted", "std::string text"});  // Enter was pressed
}

void LineEdit::guiInput(const Input& in) {
    Control::guiInput(in);
    if (disabled || !in.focused) return;
    std::string before = text;
    auto fits = [&](const std::string& more) {
        if (maxLength <= 0) return true;
        int chars = 0;
        for (unsigned char c : text + more) chars += (c & 0xC0) != 0x80;  // count characters, not bytes
        return chars <= maxLength;
    };
    for (int cp = GetCharPressed(); cp > 0; cp = GetCharPressed()) {
        int len = 0;
        const char* utf8 = CodepointToUTF8(cp, &len);
        std::string ch(utf8, static_cast<size_t>(len));
        if (fits(ch)) text += ch;
    }
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) ||
                IsKeyDown(KEY_RIGHT_SUPER);
    if (ctrl && IsKeyPressed(KEY_V)) {
        if (const char* clip = GetClipboardText()) {
            std::string paste;
            for (const char* c = clip; *c; ++c)
                if (*c != '\n' && *c != '\r') paste += *c;
            if (fits(paste)) text += paste;
        }
    }
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !text.empty()) {
        size_t cut = text.size() - 1;
        while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) --cut;  // a whole character
        text.erase(cut);
    }
    if (text != before) emitSignal("text_changed", text);
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) emitSignal("text_submitted", text);
}

void LineEdit::draw(const DrawOptions&) {
    Rectangle r = screenRect();
    BoxStyle look = style;
    if (hasFocus()) {
        look.borderColor = focusColor;
        look.borderWidth = std::max(look.borderWidth, 2.0f);
    }
    look.draw(r, dimmed(style.color, disabled));
    float size = static_cast<float>(std::max(fontSize, 1)) * scaleY();
    float pad = 8 * scaleY();
    float room = r.width - 2 * pad - 4;
    bool empty = text.empty();
    std::string shown = empty ? placeholder : text;
    // Too long: show the end of it (where the typing is)
    while (!shown.empty() && measure(shown, size).x > room) {
        size_t cut = 1;
        while (cut < shown.size() && (static_cast<unsigned char>(shown[cut]) & 0xC0) == 0x80) ++cut;
        shown.erase(0, cut);
    }
    raylib::Vector2 m = measure(shown.empty() ? "M" : shown, size);
    float ty = r.y + (r.height - m.y) / 2;
    drawString(shown, {r.x + pad, ty}, size, dimmed(empty ? placeholderColor : textColor, disabled));
    if (hasFocus() && std::fmod(GetTime(), 1.0) < 0.55) {
        float cx = r.x + pad + (empty ? 0 : measure(shown, size).x) + 1;
        DrawRectangleRec({cx, ty, std::max(1.0f, 2 * scaleY()), m.y}, textColor);
    }
}

// ---------------------------------------------------------------------------
// Containers
// ---------------------------------------------------------------------------
void BoxContainer::getProperties(std::vector<Property>& out) {
    Control::getProperties(out);
    out.push_back({vertical() ? "VBoxContainer" : "HBoxContainer", PropKind::Section});
    out.push_back({"separation", PropKind::Float, &separation, 0.1f});
    out.push_back({"fill", PropKind::Bool, &fill});
}

void BoxContainer::layout() {
    float at = 0;
    bool any = false;
    for (auto& c : children()) {
        auto* n = c->as<Node2D>();
        if (!n || !n->visible) continue;
        auto* ctl = n->as<Control>();
        Rectangle lr = n->localRect();
        if (any) at += separation;
        any = true;
        if (vertical()) {
            n->position = raylib::Vector2(0 - (ctl ? 0 : lr.x), at - (ctl ? 0 : lr.y));
            if (fill && ctl) ctl->size.x = size.x;
            at += ctl ? ctl->size.y : lr.height;
        } else {
            n->position = raylib::Vector2(at - (ctl ? 0 : lr.x), 0 - (ctl ? 0 : lr.y));
            if (fill && ctl) ctl->size.y = size.y;
            at += ctl ? ctl->size.x : lr.width;
        }
    }
    if (any) (vertical() ? size.y : size.x) = at;  // grow (or shrink) to fit them
}

void BoxContainer::draw(const DrawOptions& options) {
    layout();
    if (options.editor) {  // the box itself is invisible in the game; show where it is
        Color c = Fade(gizmoColors().collisionShapeDisabled, 0.6f);
        drawLocalRect(*this, localRect(), BLANK, c, 1 / options.zoom);
    }
}

// ---------------------------------------------------------------------------
// The mouse and keyboard
// ---------------------------------------------------------------------------
void processGui(SceneTree& tree) {
    if (isHeadless()) return;
    // Every Control, in the order they're drawn: the world first (scene, then
    // autoloads), then the canvas layers from low to high.
    std::vector<Control*> world;
    std::vector<std::pair<int, std::vector<Control*>>> layers;
    std::function<void(Node*, std::vector<Control*>&)> walk = [&](Node* n, std::vector<Control*>& into) {
        if (auto* n2 = n->as<Node2D>(); n2 && !n2->visible) return;
        if (auto* layer = n->as<CanvasLayer>(); layer && &into == &world) {
            layers.push_back({layer->layer, {}});
            size_t mine = layers.size() - 1;
            std::vector<Control*> inner;
            for (auto& c : n->children()) walk(c.get(), inner);
            layers[mine].second = std::move(inner);
            return;
        }
        if (auto* ctl = n->as<Control>()) into.push_back(ctl);
        for (auto& c : n->children()) walk(c.get(), into);
    };
    if (tree.root()) walk(tree.root(), world);
    for (auto& a : tree.autoloads()) walk(a.get(), world);
    std::stable_sort(layers.begin(), layers.end(), [](auto& a, auto& b) { return a.first < b.first; });
    std::vector<Control*> all = world;
    for (auto& [layer, list] : layers) all.insert(all.end(), list.begin(), list.end());

    raylib::Vector2 worldMouse = tree.mouseWorld();
    auto localMouse = [&](Control* c) {
        return c->toLocal(canvasLayerOf(c) ? Vector2(tree.mouseScreen) : Vector2(worldMouse));
    };

    // The one under the mouse, on top
    Control* hot = nullptr;
    for (auto it = all.rbegin(); it != all.rend(); ++it) {
        Control* c = *it;
        if (c->disabled || !c->takesMouse()) continue;
        raylib::Vector2 m = localMouse(c);
        if (m.x >= 0 && m.y >= 0 && m.x < c->size.x && m.y < c->size.y) {
            hot = c;
            break;
        }
    }
    tree.guiHot.set(hot);

    bool pressedNow = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool releasedNow = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    bool down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    if (tree.testMouse >= 0) {  // (a test clicking for itself)
        down = tree.testMouse == 1;
        pressedNow = down && !tree.testMouseWasDown;
        releasedNow = !down && tree.testMouseWasDown;
        tree.testMouseWasDown = down;
    }
    if (pressedNow) {
        tree.guiActive.set(hot);
        tree.guiFocus.set(hot && hot->takesFocus() ? hot : nullptr);
    }
    if (tree.guiFocus.get() && IsKeyPressed(KEY_ESCAPE)) tree.guiFocus.set(nullptr);
    Node* active = tree.guiActive.get();
    Node* focus = tree.guiFocus.get();

    for (Control* c : all) {
        Control::Input in;
        in.mouse = localMouse(c);
        in.hovered = c == hot;
        in.pressed = pressedNow && c == active;
        in.held = c == active && (down || releasedNow);
        in.released = releasedNow && c == active;
        in.focused = c == focus;
        c->guiInput(in);  // (may emit signals that free nodes: only flags are set on them, freeing waits)
    }
    // Ones that aren't drawn (hidden) lose the mouse
    if (releasedNow || !down) tree.guiActive.set(nullptr);
}

}  // namespace s2d
