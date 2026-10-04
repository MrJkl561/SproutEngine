#include "nodes.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>

#include "assets.hpp"
#include "scene_tree.hpp"
#include "script.hpp"

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
    out.push_back({"centered", PropKind::Bool, &centered});
    out.push_back({"offset", PropKind::Vec2, &offset, 1.0f});
    out.push_back({"flip_h", PropKind::Bool, &flipH});
    out.push_back({"flip_v", PropKind::Bool, &flipV});
    out.push_back({"modulate", PropKind::Color, &modulate});
}

Rectangle Sprite2D::localRect() const {
    raylib::Texture* tex = Assets::texture(texture);
    float w = tex ? static_cast<float>(tex->width) : 0.0f;
    float h = tex ? static_cast<float>(tex->height) : 0.0f;
    return {offset.x - (centered ? w / 2 : 0), offset.y - (centered ? h / 2 : 0), w, h};
}

void Sprite2D::draw(const DrawOptions& options) {
    raylib::Texture* tex = Assets::texture(texture);
    if (!tex) {
        // No texture yet: show a placeholder in the editor so the node can be found.
        if (options.editor) {
            drawLocalRect(*this, {-16, -16, 32, 32}, Fade(MAGENTA, 0.25f), MAGENTA, 1.0f / options.zoom);
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
    Rectangle src{0, 0, (flipH != mirrorX) ? -w : w, (flipV != mirrorY) ? -h : h};
    Rectangle dst{topLeft.x, topLeft.y, w * std::fabs(g.scale.x), h * std::fabs(g.scale.y)};
    DrawTexturePro(*tex, src, dst, {0, 0}, g.rotation, modulate);
}

// ---------------------------------------------------------------------------
// ColorRect
// ---------------------------------------------------------------------------
void ColorRect::getProperties(std::vector<Property>& out) {
    Node2D::getProperties(out);
    out.push_back({"ColorRect", PropKind::Section});
    out.push_back({"size", PropKind::Vec2, &size, 1.0f});
    out.push_back({"color", PropKind::Color, &color});
}

void ColorRect::draw(const DrawOptions&) { drawLocalRect(*this, localRect(), color, BLANK, 0); }

// ---------------------------------------------------------------------------
// Label
// ---------------------------------------------------------------------------
void Label::getProperties(std::vector<Property>& out) {
    Node2D::getProperties(out);
    out.push_back({"Label", PropKind::Section});
    out.push_back({"text", PropKind::String, &text});
    out.push_back({"font_size", PropKind::Int, &fontSize, 0.2f});
    out.push_back({"color", PropKind::Color, &color});
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
                         Color{190, 120, 255, 220});
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
    Color c = disabled ? Color{150, 150, 150, 255} : Color{60, 150, 255, 255};
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

// ---------------------------------------------------------------------------
// Node factory
// ---------------------------------------------------------------------------
namespace {

const std::vector<std::pair<std::string, std::function<std::unique_ptr<Node>()>>>& factories() {
    static const std::vector<std::pair<std::string, std::function<std::unique_ptr<Node>()>>> list = {
        {"Node", [] { return std::make_unique<Node>(); }},
        {"Node2D", [] { return std::make_unique<Node2D>(); }},
        {"Sprite2D", [] { return std::make_unique<Sprite2D>(); }},
        {"ColorRect", [] { return std::make_unique<ColorRect>(); }},
        {"Label", [] { return std::make_unique<Label>(); }},
        {"Camera2D", [] { return std::make_unique<Camera2D>(); }},
        {"CollisionShape2D", [] { return std::make_unique<CollisionShape2D>(); }},
        {"Area2D", [] { return std::make_unique<Area2D>(); }},
        {"StaticBody2D", [] { return std::make_unique<StaticBody2D>(); }},
        {"CharacterBody2D", [] { return std::make_unique<CharacterBody2D>(); }},
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
