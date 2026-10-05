// The 2D node types, modelled on Godot's: every scene is a tree of nodes, and
// each node can have one C++ script attached to it (see script.hpp).
//
//   Node              - the base of everything; has a name, children and a script
//   Node2D            - a node with a position, rotation and scale
//   Sprite2D          - draws a texture (or one frame of a sprite sheet)
//   AnimatedSprite2D  - plays named animations from a sprite sheet
//   ColorRect         - draws a solid rectangle (handy for blocking out levels)
//   Label             - draws text
//   Camera2D          - decides what part of the world the game shows
//   CollisionShape2D  - a rectangle shape, used by the physics nodes below
//   Area2D            - detects when bodies or other areas overlap it
//   StaticBody2D      - a solid thing that doesn't move (walls, floors)
//   CharacterBody2D   - a thing moved by code that collides with static bodies
#pragma once

#include <raylib-cpp.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace s2d {

class SceneTree;
class Script;

// ---------------------------------------------------------------------------
// Properties: each node lists its editable fields, which is all the inspector
// and the scene files need to know about it.
// ---------------------------------------------------------------------------
enum class PropKind { Section, Bool, Int, Float, Vec2, Color, String, Texture, Scene, Animations, AnimationName };

struct Property {
    std::string name;
    PropKind kind = PropKind::Section;
    void* ptr = nullptr;  // points at the field inside the node
    float step = 1.0f;    // how much one pixel of mouse drag changes a number
};

// Where a Node2D is in the world, after all its parents are taken into account.
struct Xform {
    raylib::Vector2 position{0, 0};
    float rotation = 0;  // degrees
    raylib::Vector2 scale{1, 1};
};

// One animation of an AnimatedSprite2D: frames of its sprite sheet, counted
// left to right and top to bottom from 0, written like "0-3" or "4, 5, 7".
struct SpriteAnimation {
    std::string name = "default";
    std::string frames = "0";
    float fps = 8;
    bool loop = true;

    std::vector<int> frameList() const;  // "0-3, 6" -> {0, 1, 2, 3, 6}
    static std::string framesText(const std::vector<int>& list);  // {0, 1, 2, 3, 6} -> "0-3, 6"
    // Which entry of the frame list shows `seconds` into the animation.
    size_t indexAt(double seconds, size_t count) const;
};

struct DrawOptions {
    bool editor = false;  // true: also draw gizmos (camera frames, collision shapes)
    float zoom = 1.0f;    // view zoom, used to keep gizmo lines one pixel wide
};

// ---------------------------------------------------------------------------
class Node {
public:
    std::string name;
    std::string scriptName;          // the registered script class, "" for none
    std::unique_ptr<Script> script;  // only exists while the game is playing
    // The script's variables as set in the inspector: name -> value as JSON text
    // (e.g. "speed" -> "250"). Only values changed from the script's defaults.
    std::map<std::string, std::string> scriptVars;
    bool collapsed = false;          // editor only: children hidden in the scene dock

    // Instanced scenes (like Godot's): a node whose sceneFile is set is the root
    // of another scene file, e.g. "scenes/player.scene.json". Its children come
    // from that file; only the root's changed properties are saved with it.
    std::string sceneFile;
    bool inInstance = false;       // part of an instanced scene (not its root): edit it in its own file
    bool instanceMissing = false;  // sceneFile couldn't be loaded; this is a stand-in

    Node();
    virtual ~Node();
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    virtual const char* typeName() const { return "Node"; }
    virtual void getProperties(std::vector<Property>& out);
    virtual void draw(const DrawOptions&) {}
    // Called every frame while the game runs (before scripts), for nodes that
    // do things by themselves, like AnimatedSprite2D.
    virtual void update(float /*delta*/) {}

    // Tree
    Node* parent() const { return parent_; }
    const std::vector<std::unique_ptr<Node>>& children() const { return children_; }
    Node* addChild(std::unique_ptr<Node> child, int index = -1);
    std::unique_ptr<Node> detach();  // takes this node (and its children) out of its parent
    int indexInParent() const;
    bool isAncestorOf(const Node* other) const;
    Node* root();

    Node* findNode(const std::string& nodeName);  // searches this node and everything below it
    Node* getNode(const std::string& path);       // "Child", "Child/Grandchild", "../Sibling"

    template <class T> T* as() { return dynamic_cast<T*>(this); }
    template <class T> T* getChild() {
        for (auto& c : children_)
            if (auto* t = dynamic_cast<T*>(c.get())) return t;
        return nullptr;
    }

    void queueFree() { queuedForDeletion_ = true; }
    bool isQueuedForDeletion() const { return queuedForDeletion_; }

    SceneTree* tree();  // nullptr while not in a scene tree

private:
    friend class SceneTree;
    Node* parent_ = nullptr;
    std::vector<std::unique_ptr<Node>> children_;
    SceneTree* tree_ = nullptr;  // only set on the root node
    bool queuedForDeletion_ = false;
};

// ---------------------------------------------------------------------------
class Node2D : public Node {
public:
    raylib::Vector2 position{0, 0};
    float rotation = 0;  // degrees, clockwise (y points down, like Godot)
    raylib::Vector2 scale{1, 1};
    bool visible = true;

    const char* typeName() const override { return "Node2D"; }
    void getProperties(std::vector<Property>& out) override;

    Xform globalXform() const;
    raylib::Vector2 globalPosition() const { return globalXform().position; }
    void setGlobalPosition(Vector2 world);
    float globalRotation() const { return globalXform().rotation; }

    raylib::Vector2 toGlobal(Vector2 local) const;  // a point in this node's space -> world
    raylib::Vector2 toLocal(Vector2 world) const;   // world -> this node's space
    bool isVisibleInTree() const;
    Node2D* parent2D() const;  // the nearest Node2D above this one

    // The node's clickable area in its own space (width 0 = just a point).
    virtual Rectangle localRect() const { return {0, 0, 0, 0}; }
};

class Sprite2D : public Node2D {
public:
    std::string texture;  // path inside the project, e.g. "assets/icon.png"
    bool centered = true;
    raylib::Vector2 offset{0, 0};
    bool flipH = false;
    bool flipV = false;
    raylib::Color modulate = WHITE;
    // A sprite sheet: the texture is a grid of hframes x vframes pictures, and
    // only `frame` is shown (0 = top left, counting along each row).
    int hframes = 1;
    int vframes = 1;
    int frame = 0;

    const char* typeName() const override { return "Sprite2D"; }
    void getProperties(std::vector<Property>& out) override;
    void draw(const DrawOptions&) override;
    Rectangle localRect() const override;

protected:
    Rectangle frameRect(int index) const;  // that frame's part of the texture
    virtual int shownFrame(const DrawOptions&) const { return frame; }
};

// Plays animations from a sprite sheet (set hframes/vframes for its grid).
//   auto* sprite = getNode("Sprite")->as<AnimatedSprite2D>();
//   sprite->play("run");
class AnimatedSprite2D : public Sprite2D {
public:
    std::vector<SpriteAnimation> animations{SpriteAnimation{}};
    std::string animation = "default";  // the one playing (or shown)
    bool autoplay = true;                // start playing it when the game starts
    float speedScale = 1.0f;

    const char* typeName() const override { return "AnimatedSprite2D"; }
    void getProperties(std::vector<Property>& out) override;
    void update(float delta) override;

    // Plays an animation from its first frame (or keeps playing it, if it's the
    // one already playing). "" = the current animation.
    void play(const std::string& name = "");
    void stop();
    bool isPlaying() const { return playing_; }
    bool isFinished() const { return finished_; }  // a non-looping animation reached its end
    const SpriteAnimation* findAnimation(const std::string& name) const;
    SpriteAnimation* findAnimation(const std::string& name);

    // Editor only (not saved): the Animation panel shows its animation or frame
    // in the viewport through these, for as long as it keeps refreshing them.
    std::string editorPreviewAnimation;
    int editorPreviewFrame = -1;  // -1: play editorPreviewAnimation
    double editorPreviewStart = 0, editorPreviewUntil = 0;

protected:
    int shownFrame(const DrawOptions& options) const override;

private:
    bool started_ = false, playing_ = false, finished_ = false;
    float time_ = 0;  // seconds into the animation
    size_t index_ = 0;  // which entry of the frame list is showing
};

class ColorRect : public Node2D {
public:
    raylib::Vector2 size{64, 64};
    raylib::Color color{70, 130, 180, 255};

    const char* typeName() const override { return "ColorRect"; }
    void getProperties(std::vector<Property>& out) override;
    void draw(const DrawOptions&) override;
    Rectangle localRect() const override { return {0, 0, size.x, size.y}; }
};

class Label : public Node2D {
public:
    std::string text = "Label";
    int fontSize = 20;
    raylib::Color color = WHITE;

    const char* typeName() const override { return "Label"; }
    void getProperties(std::vector<Property>& out) override;
    void draw(const DrawOptions&) override;
    Rectangle localRect() const override;
};

class Camera2D : public Node2D {
public:
    float zoom = 1.0f;
    bool current = true;

    const char* typeName() const override { return "Camera2D"; }
    void getProperties(std::vector<Property>& out) override;
    void draw(const DrawOptions&) override;
};

class CollisionShape2D : public Node2D {
public:
    raylib::Vector2 size{32, 32};  // a rectangle centred on the node
    bool disabled = false;

    const char* typeName() const override { return "CollisionShape2D"; }
    void getProperties(std::vector<Property>& out) override;
    void draw(const DrawOptions&) override;
    Rectangle localRect() const override { return {-size.x / 2, -size.y / 2, size.x, size.y}; }

    Rectangle worldAABB() const;  // ignores rotation, like simple arcade physics
};

// Base for the nodes that use CollisionShape2D children.
class CollisionObject2D : public Node2D {
public:
    std::vector<Rectangle> shapes() const;  // world rectangles of the enabled shape children
    bool overlaps(const CollisionObject2D& other) const;
};

class Area2D : public CollisionObject2D {
public:
    const char* typeName() const override { return "Area2D"; }
    std::vector<Node2D*> getOverlappingBodies();  // StaticBody2D and CharacterBody2D nodes
    std::vector<Node2D*> getOverlappingAreas();
};

class StaticBody2D : public CollisionObject2D {
public:
    const char* typeName() const override { return "StaticBody2D"; }
};

class CharacterBody2D : public CollisionObject2D {
public:
    raylib::Vector2 velocity{0, 0};

    const char* typeName() const override { return "CharacterBody2D"; }
    void getProperties(std::vector<Property>& out) override;

    // Moves by velocity * delta and slides along any StaticBody2D in the way.
    void moveAndSlide();
    bool isOnFloor() const { return onFloor_; }
    bool isOnWall() const { return onWall_; }
    bool isOnCeiling() const { return onCeiling_; }

private:
    bool onFloor_ = false, onWall_ = false, onCeiling_ = false;
};

// ---------------------------------------------------------------------------
// The colors the editor draws nodes' outlines with (collision shapes, camera
// frames...). The editor sets them from its theme; games never draw these.
struct GizmoColors {
    Color collisionShape{60, 150, 255, 255};
    Color collisionShapeDisabled{150, 150, 150, 255};
    Color camera{190, 120, 255, 220};
    Color missingTexture{255, 0, 255, 255};
};
GizmoColors& gizmoColors();

// Making nodes by type name (used by the "Add Node" menu and scene files).
const std::vector<std::string>& nodeTypeNames();
std::unique_ptr<Node> createNode(const std::string& typeName);

// Draws a rectangle given in a node's own space, rotated and scaled with it.
void drawLocalRect(const Node2D& node, Rectangle local, Color fill, Color outline, float lineWidth);

}  // namespace s2d
