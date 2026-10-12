#include "nodes.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>

#include <rlgl.h>

#include "assets.hpp"
#include "multiplayer.hpp"
#include "scene_tree.hpp"
#include "script.hpp"
#include "shaders.hpp"

namespace s2d {

namespace {

raylib::Vector2 rotate(Vector2 v, float degrees) {
    float r = degrees * DEG2RAD;
    float c = std::cos(r), s = std::sin(r);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}

}  // namespace

// ---------------------------------------------------------------------------
// Node
// ---------------------------------------------------------------------------
Node::Node() = default;
Node::~Node() = default;

void Node::getProperties(std::vector<Property>&) {}

void Node::setMultiplayerAuthority(int peerId, bool recursive) {
    authority_ = peerId;
    if (recursive)
        for (auto& c : children_) c->setMultiplayerAuthority(peerId, true);
}

bool Node::isMultiplayerAuthority() const { return authority_ == multiplayer().uniqueId(); }

// ---------------------------------------------------------------------------
// Signals
// ---------------------------------------------------------------------------
void Node::getSignals(std::vector<SignalInfo>& out) const {
    out.push_back({"ready", ""});         // after the node's script's ready() (children first)
    out.push_back({"tree_exiting", ""});  // the node is about to be freed
}

std::vector<SignalInfo> Node::signalList() const {
    std::vector<SignalInfo> out;
    getSignals(out);
    if (script)
        for (const SignalInfo& s : script->signals()) out.push_back(s);
    return out;
}

bool Node::hasSignal(const std::string& name) const {
    for (const SignalInfo& s : signalList())
        if (s.name == name) return true;
    return false;
}

void Node::emitSignalArgs(const std::string& name, const SignalArgs& args) {
    if (!hub_) return;  // nobody has ever connected
    auto hub = hub_;     // (kept even if a listener frees this node)
    hub->emit(name, args);
}

std::shared_ptr<SignalHub> Node::signalHub() {
    if (!hub_) hub_ = std::make_shared<SignalHub>();
    return hub_;
}

Node* Node::addChild(std::unique_ptr<Node> child, int index) {
    if (!child) return nullptr;
    child->parent_ = this;
    Node* raw = child.get();
    if (index < 0 || index > static_cast<int>(children_.size())) {
        children_.push_back(std::move(child));
    } else {
        children_.insert(children_.begin() + index, std::move(child));
    }
    if (SceneTree* t = tree()) t->nodeAdded(raw);
    return raw;
}

std::unique_ptr<Node> Node::detach() {
    if (!parent_) return nullptr;
    auto& siblings = parent_->children_;
    for (auto it = siblings.begin(); it != siblings.end(); ++it) {
        if (it->get() == this) {
            std::unique_ptr<Node> self = std::move(*it);
            siblings.erase(it);
            parent_ = nullptr;
            return self;
        }
    }
    return nullptr;
}

int Node::indexInParent() const {
    if (!parent_) return 0;
    for (size_t i = 0; i < parent_->children_.size(); ++i)
        if (parent_->children_[i].get() == this) return static_cast<int>(i);
    return -1;
}

bool Node::isAncestorOf(const Node* other) const {
    for (const Node* p = other ? other->parent_ : nullptr; p; p = p->parent_)
        if (p == this) return true;
    return false;
}

Node* Node::root() {
    Node* n = this;
    while (n->parent_) n = n->parent_;
    return n;
}

SceneTree* Node::tree() { return root()->tree_; }

Node* Node::findNode(const std::string& nodeName) {
    if (name == nodeName) return this;
    for (auto& c : children_)
        if (Node* found = c->findNode(nodeName)) return found;
    return nullptr;
}

Node* Node::getNode(const std::string& path) {
    Node* current = this;
    size_t start = 0;
    while (current && start <= path.size()) {
        size_t slash = path.find('/', start);
        std::string part = path.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (part == "..") {
            current = current->parent_;
        } else if (!part.empty() && part != ".") {
            Node* next = nullptr;
            for (auto& c : current->children_)
                if (c->name == part) { next = c.get(); break; }
            current = next;
        }
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return current;
}

// ---------------------------------------------------------------------------
// Node2D
// ---------------------------------------------------------------------------
void Node2D::getProperties(std::vector<Property>& out) {
    Node::getProperties(out);
    out.push_back({"Node2D", PropKind::Section});
    out.push_back({"position", PropKind::Vec2, &position, 1.0f});
    out.push_back({"rotation", PropKind::Float, &rotation, 1.0f});
    out.push_back({"scale", PropKind::Vec2, &scale, 0.01f});
    out.push_back({"visible", PropKind::Bool, &visible});
}

void Node2D::addShaderProperties(std::vector<Property>& out) {
    out.push_back({"Shader", PropKind::Section});
    out.push_back({"shader", PropKind::Shader, &shader});
    const std::vector<ShaderUniform>* uniforms = shader.empty() ? nullptr : Shaders::uniforms(shader);
    if (!uniforms || uniforms->empty()) return;
    out.push_back({"Shader Parameters", PropKind::Section});
    for (const ShaderUniform& u : *uniforms) {
        // A value for each (the shader's default, the first time)
        auto [it, added] = shaderParams.try_emplace(u.name);
        ShaderParam& p = it->second;
        if (added) {
            p.f = u.value[0];
            p.i = static_cast<int>(u.value[0]);
            p.b = u.value[0] != 0;
            p.v = raylib::Vector2(u.value[0], u.value[1]);
            p.c = ::Color{static_cast<unsigned char>(std::clamp(u.value[0], 0.0f, 1.0f) * 255 + 0.5f),
                   static_cast<unsigned char>(std::clamp(u.value[1], 0.0f, 1.0f) * 255 + 0.5f),
                   static_cast<unsigned char>(std::clamp(u.value[2], 0.0f, 1.0f) * 255 + 0.5f),
                   static_cast<unsigned char>(std::clamp(u.value[3], 0.0f, 1.0f) * 255 + 0.5f)};
            p.texture = u.texture;
        }
        std::string name = "shader_params/" + u.name;  // (saved under this name; the Inspector shows just u.name)
        switch (u.kind) {
            case ShaderUniform::Float: out.push_back({name, PropKind::Float, &p.f, u.step}); break;
            case ShaderUniform::Int: out.push_back({name, PropKind::Int, &p.i, u.step}); break;
            case ShaderUniform::Bool: out.push_back({name, PropKind::Bool, &p.b}); break;
            case ShaderUniform::Vec2: out.push_back({name, PropKind::Vec2, &p.v, u.step}); break;
            case ShaderUniform::Color: out.push_back({name, PropKind::Color, &p.c}); break;
            case ShaderUniform::Texture: out.push_back({name, PropKind::Texture, &p.texture}); break;
        }
    }
}

void Node2D::setShaderParam(const std::string& name, float value) {
    ShaderParam& p = shaderParams[name];
    p.f = value;
    p.i = static_cast<int>(value);
    p.b = value != 0;
}

void Node2D::setShaderParam(const std::string& name, int value) {
    ShaderParam& p = shaderParams[name];
    p.f = static_cast<float>(value);
    p.i = value;
    p.b = value != 0;
}

void Node2D::setShaderParam(const std::string& name, bool value) {
    ShaderParam& p = shaderParams[name];
    p.f = value ? 1.0f : 0.0f;
    p.i = value ? 1 : 0;
    p.b = value;
}

void Node2D::setShaderParam(const std::string& name, ::Vector2 value) { shaderParams[name].v = value; }
void Node2D::setShaderParam(const std::string& name, ::Color value) { shaderParams[name].c = value; }
void Node2D::setShaderTexture(const std::string& name, const std::string& texturePath) {
    shaderParams[name].texture = texturePath;
}

Node2D* Node2D::parent2D() const {
    for (Node* p = parent(); p; p = p->parent())
        if (auto* n = dynamic_cast<Node2D*>(p)) return n;
    return nullptr;
}

Xform Node2D::globalXform() const {
    Xform local{position, rotation, scale};
    Node2D* p = parent2D();
    if (!p) return local;
    Xform pg = p->globalXform();
    Xform g;
    g.position = pg.position + rotate(position * pg.scale, pg.rotation);
    g.rotation = pg.rotation + rotation;
    g.scale = pg.scale * scale;
    return g;
}

void Node2D::setGlobalPosition(Vector2 world) {
    Node2D* p = parent2D();
    position = p ? p->toLocal(world) : raylib::Vector2(world);
}

raylib::Vector2 Node2D::toGlobal(Vector2 local) const {
    Xform g = globalXform();
    return g.position + rotate(raylib::Vector2(local) * g.scale, g.rotation);
}

raylib::Vector2 Node2D::toLocal(Vector2 world) const {
    Xform g = globalXform();
    raylib::Vector2 d = rotate(raylib::Vector2(world) - g.position, -g.rotation);
    float sx = std::fabs(g.scale.x) < 1e-6f ? 1e-6f : g.scale.x;
    float sy = std::fabs(g.scale.y) < 1e-6f ? 1e-6f : g.scale.y;
    return {d.x / sx, d.y / sy};
}

bool Node2D::isVisibleInTree() const {
    if (!visible) return false;
    Node2D* p = parent2D();
    return p ? p->isVisibleInTree() : true;
}

void drawLocalRect(const Node2D& node, Rectangle local, Color fill, Color outline, float lineWidth) {
    raylib::Vector2 corners[4] = {
        node.toGlobal({local.x, local.y}),
        node.toGlobal({local.x + local.width, local.y}),
        node.toGlobal({local.x + local.width, local.y + local.height}),
        node.toGlobal({local.x, local.y + local.height}),
    };
    if (fill.a > 0) {
        // raylib wants counter-clockwise triangles; draw both windings so a
        // negative scale (mirrored rectangle) still shows up.
        DrawTriangle(corners[0], corners[3], corners[2], fill);
        DrawTriangle(corners[0], corners[2], corners[1], fill);
        DrawTriangle(corners[0], corners[2], corners[3], fill);
        DrawTriangle(corners[0], corners[1], corners[2], fill);
    }
    if (outline.a > 0 && lineWidth > 0) {
        for (int i = 0; i < 4; ++i) DrawLineEx(corners[i], corners[(i + 1) % 4], lineWidth, outline);
    }
}

// ---------------------------------------------------------------------------
// Sprite2D
// ---------------------------------------------------------------------------
void Sprite2D::getProperties(std::vector<Property>& out) {
    Node2D::getProperties(out);
    out.push_back({"Sprite2D", PropKind::Section});
    out.push_back({"texture", PropKind::Texture, &texture});
    out.push_back({"hframes", PropKind::Int, &hframes, 0.05f});
    out.push_back({"vframes", PropKind::Int, &vframes, 0.05f});
    out.push_back({"frame", PropKind::Int, &frame, 0.1f});
    out.push_back({"centered", PropKind::Bool, &centered});
    out.push_back({"offset", PropKind::Vec2, &offset, 1.0f});
    out.push_back({"flip_h", PropKind::Bool, &flipH});
    out.push_back({"flip_v", PropKind::Bool, &flipV});
    out.push_back({"modulate", PropKind::Color, &modulate});
    if (!dynamic_cast<AnimatedSprite2D*>(this)) addShaderProperties(out);  // (an AnimatedSprite2D adds them last)
}

Rectangle Sprite2D::frameRect(int index) const {
    raylib::Texture* tex = Assets::texture(texture);
    if (!tex) return {0, 0, 0, 0};
    int cols = std::max(hframes, 1), rows = std::max(vframes, 1);
    float w = static_cast<float>(tex->width) / cols, h = static_cast<float>(tex->height) / rows;
    index = std::clamp(index, 0, cols * rows - 1);
    return {(index % cols) * w, (index / cols) * h, w, h};
}

Rectangle Sprite2D::localRect() const {
    Rectangle cell = frameRect(0);
    float w = cell.width, h = cell.height;
    return {offset.x - (centered ? w / 2 : 0), offset.y - (centered ? h / 2 : 0), w, h};
}

void Sprite2D::draw(const DrawOptions& options) {
    raylib::Texture* tex = Assets::texture(texture);
    if (!tex) {
        // No texture yet: show a placeholder in the editor so the node can be found.
        if (options.editor) {
            Color c = gizmoColors().missingTexture;
            drawLocalRect(*this, {-16, -16, 32, 32}, Fade(c, 0.25f), c, 1.0f / options.zoom);
        }
        return;
    }
    Xform g = globalXform();
    Rectangle r = localRect();
    float w = r.width, h = r.height;
    // Start from the corner that ends up top-left after mirroring, and flip the
    // texture instead of using a negative size (raylib can't draw those).
    bool mirrorX = g.scale.x < 0, mirrorY = g.scale.y < 0;
    raylib::Vector2 topLeft = toGlobal({mirrorX ? r.x + w : r.x, mirrorY ? r.y + h : r.y});
    Rectangle cell = frameRect(shownFrame(options));
    Rectangle src{cell.x, cell.y, (flipH != mirrorX) ? -w : w, (flipV != mirrorY) ? -h : h};
    Rectangle dst{topLeft.x, topLeft.y, w * std::fabs(g.scale.x), h * std::fabs(g.scale.y)};
    DrawTexturePro(*tex, src, dst, {0, 0}, g.rotation, modulate);
}

// ---------------------------------------------------------------------------
// AnimatedSprite2D
// ---------------------------------------------------------------------------
std::vector<int> SpriteAnimation::frameList() const {
    std::vector<int> out;
    std::string part;
    auto flush = [&] {
        size_t dash = part.find('-', 1);
        try {
            if (dash != std::string::npos) {
                int a = std::stoi(part.substr(0, dash)), b = std::stoi(part.substr(dash + 1));
                for (int i = a; a <= b ? i <= b : i >= b; i += a <= b ? 1 : -1) out.push_back(i);
            } else if (!part.empty()) {
                out.push_back(std::stoi(part));
            }
        } catch (const std::exception&) {
            // not a number: skipped
        }
        part.clear();
    };
    for (char c : frames) {
        if (c == ',' || c == ' ') flush();
        else part += c;
    }
    flush();
    if (out.size() > 4096) out.resize(4096);  // "0-99999" by mistake
    return out;
}

std::string SpriteAnimation::framesText(const std::vector<int>& list) {
    // Runs of 3 or more counting up become "a-b".
    std::string out;
    for (size_t i = 0; i < list.size();) {
        size_t j = i;
        while (j + 1 < list.size() && list[j + 1] == list[j] + 1) ++j;
        if (!out.empty()) out += ", ";
        if (j - i >= 2) {
            out += std::to_string(list[i]) + "-" + std::to_string(list[j]);
            i = j + 1;
        } else {
            out += std::to_string(list[i]);
            ++i;
        }
    }
    return out;
}

size_t SpriteAnimation::indexAt(double seconds, size_t count) const {
    if (count == 0 || fps <= 0) return 0;
    size_t step = static_cast<size_t>(std::max(0.0, seconds) * fps);
    return loop ? step % count : std::min(step, count - 1);
}

void AnimatedSprite2D::getProperties(std::vector<Property>& out) {
    Sprite2D::getProperties(out);
    out.push_back({"AnimatedSprite2D", PropKind::Section});
    out.push_back({"animation", PropKind::AnimationName, &animation});
    out.push_back({"autoplay", PropKind::Bool, &autoplay});
    out.push_back({"speed_scale", PropKind::Float, &speedScale, 0.01f});
    out.push_back({"animations", PropKind::Animations, &animations});
    addShaderProperties(out);
}

const SpriteAnimation* AnimatedSprite2D::findAnimation(const std::string& name) const {
    for (auto& a : animations)
        if (a.name == name) return &a;
    return nullptr;
}

SpriteAnimation* AnimatedSprite2D::findAnimation(const std::string& name) {
    for (auto& a : animations)
        if (a.name == name) return &a;
    return nullptr;
}

void AnimatedSprite2D::play(const std::string& name) {
    started_ = true;
    std::string next = name.empty() ? animation : name;
    if (playing_ && next == animation) return;  // already playing it: carry on
    animation = next;
    playing_ = true;
    finished_ = false;
    time_ = 0;
    index_ = 0;
    if (const SpriteAnimation* a = findAnimation(animation)) {
        auto list = a->frameList();
        if (!list.empty()) frame = list.front();
    }
}

void AnimatedSprite2D::stop() {
    started_ = true;
    playing_ = false;
}

void AnimatedSprite2D::getSignals(std::vector<SignalInfo>& out) const {
    Sprite2D::getSignals(out);
    out.push_back({"animation_finished", ""});  // a non-looping animation reached its last frame
    out.push_back({"animation_looped", ""});    // a looping animation started over
    out.push_back({"frame_changed", ""});
}

void AnimatedSprite2D::update(float delta) {
    if (!started_) {
        started_ = true;
        if (autoplay) play();
    }
    const SpriteAnimation* a = findAnimation(animation);
    if (!playing_ || !a || a->fps <= 0) return;
    auto list = a->frameList();
    if (list.empty()) return;
    time_ += delta * speedScale;
    float perFrame = 1.0f / a->fps;
    int before = frame;
    bool looped = false, finished = false;
    while (time_ >= perFrame) {
        time_ -= perFrame;
        if (index_ + 1 < list.size()) {
            ++index_;
        } else if (a->loop) {
            index_ = 0;
            looped = true;
        } else {
            playing_ = false;
            finished_ = true;
            finished = true;
            break;
        }
    }
    frame = list[std::min(index_, list.size() - 1)];
    if (frame != before) emitSignal("frame_changed");
    if (looped) emitSignal("animation_looped");
    if (finished) emitSignal("animation_finished");
}

int AnimatedSprite2D::shownFrame(const DrawOptions& options) const {
    // The editor's Animation panel is showing an animation or a frame.
    if (options.editor && GetTime() < editorPreviewUntil) {
        if (editorPreviewFrame >= 0) return editorPreviewFrame;
        if (const SpriteAnimation* a = findAnimation(editorPreviewAnimation)) {
            auto list = a->frameList();
            if (!list.empty()) return list[a->indexAt((GetTime() - editorPreviewStart) * speedScale, list.size())];
        }
        return frame;
    }
    // In the editor, preview the animation (when it would autoplay) by the clock.
    if (options.editor && autoplay) {
        if (const SpriteAnimation* a = findAnimation(animation); a && a->fps > 0) {
            auto list = a->frameList();
            if (!list.empty()) {
                size_t i = static_cast<size_t>(GetTime() * a->fps * speedScale) % list.size();
                return list[i];
            }
        }
    }
    return frame;
}

// ---------------------------------------------------------------------------
// ColorRect
// ---------------------------------------------------------------------------
void ColorRect::getProperties(std::vector<Property>& out) {
    Node2D::getProperties(out);
    out.push_back({"ColorRect", PropKind::Section});
    out.push_back({"size", PropKind::Vec2, &size, 1.0f});
    out.push_back({"color", PropKind::Color, &color});
    addShaderProperties(out);
}

void ColorRect::draw(const DrawOptions&) {
    if (shader.empty()) {
        drawLocalRect(*this, localRect(), color, BLANK, 0);
        return;
    }
    // With a shader: a quad whose UV goes from 0 to 1 across the rectangle
    // (a plain rectangle's UV is a single point of raylib's white texture).
    Rectangle r = localRect();
    raylib::Vector2 c[4] = {toGlobal({r.x, r.y}), toGlobal({r.x + r.width, r.y}),
                            toGlobal({r.x + r.width, r.y + r.height}), toGlobal({r.x, r.y + r.height})};
    float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    rlSetTexture(rlGetTextureIdDefault());
    rlBegin(RL_QUADS);
    rlColor4ub(color.r, color.g, color.b, color.a);
    for (int order : {0, 3, 2, 1, 0, 1, 2, 3}) {  // both windings, so a mirrored one shows too
        rlTexCoord2f(uv[order][0], uv[order][1]);
        rlVertex2f(c[order].x, c[order].y);
    }
    rlEnd();
    rlSetTexture(0);
}

// ---------------------------------------------------------------------------
// Label
// ---------------------------------------------------------------------------
void Label::getProperties(std::vector<Property>& out) {
    Node2D::getProperties(out);
    out.push_back({"Label", PropKind::Section});
    out.push_back({"text", PropKind::String, &text});
    out.push_back({"font_size", PropKind::Int, &fontSize, 0.2f});
    out.push_back({"color", PropKind::Color, &color});
    addShaderProperties(out);
}

Rectangle Label::localRect() const {
    float size = static_cast<float>(std::max(fontSize, 1));
    raylib::Vector2 m = MeasureTextEx(GetFontDefault(), text.c_str(), size, size / 10);
    return {0, 0, std::max(m.x, 8.0f), size};
}

void Label::draw(const DrawOptions&) {
    Xform g = globalXform();
    float size = static_cast<float>(std::max(fontSize, 1)) * std::fabs(g.scale.y);
    ::DrawTextPro(GetFontDefault(), text.c_str(), ::Vector2{g.position.x, g.position.y}, ::Vector2{0, 0}, g.rotation, size, size / 10, color);
}

// ---------------------------------------------------------------------------
// Camera2D
// ---------------------------------------------------------------------------
void Camera2D::getProperties(std::vector<Property>& out) {
    Node2D::getProperties(out);
    out.push_back({"Camera2D", PropKind::Section});
    out.push_back({"zoom", PropKind::Float, &zoom, 0.01f});
    out.push_back({"current", PropKind::Bool, &current});
}

void Camera2D::draw(const DrawOptions& options) {
    if (!options.editor) return;
    raylib::Vector2 size{800, 450};
    if (SceneTree* t = tree()) size = t->gameSize;
    float z = zoom > 0.01f ? zoom : 0.01f;
    raylib::Vector2 half = size / (2 * z);
    raylib::Vector2 center = globalPosition();
    DrawRectangleLinesEx({center.x - half.x, center.y - half.y, half.x * 2, half.y * 2}, 2.0f / options.zoom,
                         gizmoColors().camera);
}

// ---------------------------------------------------------------------------
// CollisionShape2D and the physics bodies
// ---------------------------------------------------------------------------
void CollisionShape2D::getProperties(std::vector<Property>& out) {
    Node2D::getProperties(out);
    out.push_back({"CollisionShape2D", PropKind::Section});
    out.push_back({"size", PropKind::Vec2, &size, 1.0f});
    out.push_back({"disabled", PropKind::Bool, &disabled});
}

void CollisionShape2D::draw(const DrawOptions& options) {
    if (!options.editor) return;
    Color c = disabled ? gizmoColors().collisionShapeDisabled : gizmoColors().collisionShape;
    drawLocalRect(*this, localRect(), Fade(c, 0.3f), c, 1.5f / options.zoom);
}

Rectangle CollisionShape2D::worldAABB() const {
    Xform g = globalXform();
    float w = size.x * std::fabs(g.scale.x), h = size.y * std::fabs(g.scale.y);
    return {g.position.x - w / 2, g.position.y - h / 2, w, h};
}

std::vector<Rectangle> CollisionObject2D::shapes() const {
    std::vector<Rectangle> out;
    for (auto& c : children())
        if (auto* shape = dynamic_cast<CollisionShape2D*>(c.get()); shape && !shape->disabled)
            out.push_back(shape->worldAABB());
    return out;
}

bool CollisionObject2D::overlaps(const CollisionObject2D& other) const {
    if (&other == this) return false;
    auto mine = shapes();
    auto theirs = other.shapes();
    for (auto& a : mine)
        for (auto& b : theirs)
            if (CheckCollisionRecs(a, b)) return true;
    return false;
}

namespace {

template <class Pred>
std::vector<Node2D*> overlapping(CollisionObject2D& self, Pred pred) {
    std::vector<Node2D*> out;
    forEachNode(self.root(), [&](Node* n) {
        auto* obj = dynamic_cast<CollisionObject2D*>(n);
        if (obj && obj != &self && pred(obj) && !obj->isQueuedForDeletion() && self.overlaps(*obj))
            out.push_back(obj);
    });
    return out;
}

}  // namespace

void Area2D::getSignals(std::vector<SignalInfo>& out) const {
    CollisionObject2D::getSignals(out);
    out.push_back({"body_entered", "Node* body"});  // a StaticBody2D or CharacterBody2D came in
    out.push_back({"body_exited", "Node* body"});
    out.push_back({"area_entered", "Node* area"});  // another Area2D came in
    out.push_back({"area_exited", "Node* area"});
}

void Area2D::update(float) {
    auto track = [this](std::vector<Inside>& in, const std::vector<Node2D*>& now, const char* entered,
                        const char* exited) {
        // Gone out (or freed: then there's nothing to tell, see nodesLeaving)
        std::vector<Inside> still;
        for (const Inside& i : in) {
            if (i.life.expired()) continue;
            if (std::find(now.begin(), now.end(), i.node) != now.end()) still.push_back(i);
            else emitSignal(exited, i.node);
        }
        // Come in
        for (Node2D* n : now) {
            bool known = std::any_of(still.begin(), still.end(), [n](const Inside& i) { return i.node == n; });
            if (known) continue;
            still.push_back({n, n->lifetime()});
            emitSignal(entered, n);
        }
        in = std::move(still);
    };
    if (!isSignalConnected("body_entered") && !isSignalConnected("body_exited") && !isSignalConnected("area_entered") &&
        !isSignalConnected("area_exited")) {
        bodiesIn_.clear();  // nobody's listening: don't keep track
        areasIn_.clear();
        return;
    }
    track(bodiesIn_, getOverlappingBodies(), "body_entered", "body_exited");
    track(areasIn_, getOverlappingAreas(), "area_entered", "area_exited");
}

void Area2D::nodesLeaving(const std::vector<Node*>& leaving) {
    auto drop = [&](std::vector<Inside>& in, const char* exited) {
        for (auto it = in.begin(); it != in.end();) {
            if (std::find(leaving.begin(), leaving.end(), it->node) != leaving.end() && !it->life.expired()) {
                Node* n = it->node;
                it = in.erase(it);
                emitSignal(exited, n);
            } else {
                ++it;
            }
        }
    };
    drop(bodiesIn_, "body_exited");
    drop(areasIn_, "area_exited");
}

std::vector<Node2D*> Area2D::getOverlappingBodies() {
    return overlapping(*this, [](CollisionObject2D* o) {
        return dynamic_cast<StaticBody2D*>(o) || dynamic_cast<CharacterBody2D*>(o);
    });
}

std::vector<Node2D*> Area2D::getOverlappingAreas() {
    return overlapping(*this, [](CollisionObject2D* o) { return dynamic_cast<Area2D*>(o) != nullptr; });
}

void CharacterBody2D::getProperties(std::vector<Property>& out) {
    Node2D::getProperties(out);
    out.push_back({"CharacterBody2D", PropKind::Section});
    out.push_back({"velocity", PropKind::Vec2, &velocity, 1.0f});
}

void CharacterBody2D::moveAndSlide() {
    SceneTree* t = tree();
    float dt = t ? t->delta() : GetFrameTime();
    onFloor_ = onWall_ = onCeiling_ = false;

    std::vector<Rectangle> solids;
    forEachNode(root(), [&](Node* n) {
        if (auto* body = dynamic_cast<StaticBody2D*>(n))
            for (auto& r : body->shapes()) solids.push_back(r);
    });

    // Our shapes, relative to our global position, so we can move them around.
    raylib::Vector2 pos = globalPosition();
    std::vector<Rectangle> mine = shapes();
    for (auto& r : mine) {
        r.x -= pos.x;
        r.y -= pos.y;
    }

    auto resolve = [&](bool horizontal, float move) {
        if (horizontal) pos.x += move; else pos.y += move;
        if (move == 0) return;
        for (const Rectangle& rel : mine) {
            for (const Rectangle& s : solids) {
                Rectangle r{pos.x + rel.x, pos.y + rel.y, rel.width, rel.height};
                if (!CheckCollisionRecs(r, s)) continue;
                if (horizontal) {
                    pos.x += move > 0 ? s.x - (r.x + r.width) : (s.x + s.width) - r.x;
                    velocity.x = 0;
                    onWall_ = true;
                } else {
                    pos.y += move > 0 ? s.y - (r.y + r.height) : (s.y + s.height) - r.y;
                    if (move > 0) onFloor_ = true; else onCeiling_ = true;
                    velocity.y = 0;
                }
            }
        }
    };
    resolve(true, velocity.x * dt);
    resolve(false, velocity.y * dt);
    setGlobalPosition(pos);
}

GizmoColors& gizmoColors() {
    static GizmoColors colors;
    return colors;
}

// ---------------------------------------------------------------------------
// Multiplayer nodes
// ---------------------------------------------------------------------------
void MultiplayerSynchronizer::getProperties(std::vector<Property>& out) {
    Node::getProperties(out);
    out.push_back({"MultiplayerSynchronizer", PropKind::Section});
    out.push_back({"root_path", PropKind::String, &rootPath});
    out.push_back({"properties", PropKind::String, &properties});
    out.push_back({"interval", PropKind::Float, &interval, 0.01f});
    out.push_back({"interpolate", PropKind::Bool, &interpolate});
    out.push_back({"interpolation_delay", PropKind::Float, &interpolationDelay, 0.01f});
}

void MultiplayerSpawner::getProperties(std::vector<Property>& out) {
    Node::getProperties(out);
    out.push_back({"MultiplayerSpawner", PropKind::Section});
    out.push_back({"spawn_path", PropKind::String, &spawnPath});
}

void MultiplayerSpawner::getSignals(std::vector<SignalInfo>& out) const {
    Node::getSignals(out);
    out.push_back({"spawned", "Node* node"});    // a copy of something the server spawned arrived (clients)
    out.push_back({"despawned", "Node* node"});  // the server freed it (clients; it's freed after this)
}

// ---------------------------------------------------------------------------
// Timer
// ---------------------------------------------------------------------------
void Timer::getProperties(std::vector<Property>& out) {
    Node::getProperties(out);
    out.push_back({"Timer", PropKind::Section});
    out.push_back({"wait_time", PropKind::Float, &waitTime, 0.01f});
    out.push_back({"one_shot", PropKind::Bool, &oneShot});
    out.push_back({"autostart", PropKind::Bool, &autostart});
}

void Timer::getSignals(std::vector<SignalInfo>& out) const {
    Node::getSignals(out);
    out.push_back({"timeout", ""});
}

void Timer::start(float seconds) {
    if (seconds > 0) waitTime = seconds;
    left_ = std::max(0.001f, waitTime);
    running_ = true;
    started_ = true;
}

void Timer::stop() { running_ = false; }

void Timer::update(float delta) {
    if (!started_) {
        started_ = true;
        if (autostart) start();
    }
    if (!running_) return;
    left_ -= delta;
    if (left_ > 0) return;
    if (oneShot) running_ = false;
    else left_ = std::max(left_ + std::max(0.001f, waitTime), 0.0f);  // keeps the beat, even if a frame was late
    emitSignal("timeout");
}

// ---------------------------------------------------------------------------
// Node factory
// ---------------------------------------------------------------------------
namespace {

const std::vector<std::pair<std::string, std::function<std::unique_ptr<Node>()>>>& factories() {
    static const std::vector<std::pair<std::string, std::function<std::unique_ptr<Node>()>>> list = {
        {"Node", [] { return std::make_unique<Node>(); }},
        {"Node2D", [] { return std::make_unique<Node2D>(); }},
        {"Sprite2D", [] { return std::make_unique<Sprite2D>(); }},
        {"AnimatedSprite2D", [] { return std::make_unique<AnimatedSprite2D>(); }},
        {"ColorRect", [] { return std::make_unique<ColorRect>(); }},
        {"Label", [] { return std::make_unique<Label>(); }},
        {"Camera2D", [] { return std::make_unique<Camera2D>(); }},
        {"CollisionShape2D", [] { return std::make_unique<CollisionShape2D>(); }},
        {"Area2D", [] { return std::make_unique<Area2D>(); }},
        {"StaticBody2D", [] { return std::make_unique<StaticBody2D>(); }},
        {"CharacterBody2D", [] { return std::make_unique<CharacterBody2D>(); }},
        {"MultiplayerSynchronizer", [] { return std::make_unique<MultiplayerSynchronizer>(); }},
        {"MultiplayerSpawner", [] { return std::make_unique<MultiplayerSpawner>(); }},
        {"Timer", [] { return std::make_unique<Timer>(); }},
    };
    return list;
}

}  // namespace

const std::vector<std::string>& nodeTypeNames() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> v;
        for (auto& [name, make] : factories()) v.push_back(name);
        return v;
    }();
    return names;
}

std::unique_ptr<Node> createNode(const std::string& typeName) {
    for (auto& [name, make] : factories()) {
        if (name == typeName) {
            auto node = make();
            node->name = typeName;
            return node;
        }
    }
    return nullptr;
}

}  // namespace s2d
