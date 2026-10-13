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
//   MultiplayerSynchronizer - keeps properties the same on every player's game
//   MultiplayerSpawner      - copies the scenes the server adds to every player's game
//   Timer             - counts down and sends its "timeout" signal
//   TileMap           - a grid of tiles (tilemap.hpp)
//   CanvasLayer, Panel, Button, CheckBox, HSlider, ProgressBar, TextureRect,
//   LineEdit, VBoxContainer, HBoxContainer - the UI nodes (ui_nodes.hpp)
//
// Nodes send signals when things happen (see signals.hpp): every node sends
// "ready" and "tree_exiting"; Area2D "body_entered"..., Timer "timeout"...
#pragma once

#include <raylib-cpp.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "signals.hpp"

namespace s2d {

class SceneTree;
class Script;

// ---------------------------------------------------------------------------
// Properties: each node lists its editable fields, which is all the inspector
// and the scene files need to know about it.
// ---------------------------------------------------------------------------
enum class PropKind { Section, Bool, Int, Float, Vec2, Color, String, Texture, Scene, Animations, AnimationName, Shader, Tiles };

struct Property {
    std::string name;
    PropKind kind = PropKind::Section;
    void* ptr = nullptr;  // points at the field inside the node (Tiles: at the TileMap)
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
    Rectangle view{0, 0, 0, 0};  // the part of the world on screen (width 0: unknown), to skip what's off it
};

// A signal connection made in the editor (the Inspector's Signals list),
// saved in the scene file and connected when the game starts.
struct SavedConnection {
    std::string signal;  // e.g. "body_entered"
    std::string target;  // the node with the script, from this node: ".", "..", "../Hud"
    std::string method;  // a function of its script, marked CALLABLE(method)
    bool oneShot = false;
    bool operator==(const SavedConnection& o) const {
        return signal == o.signal && target == o.target && method == o.method && oneShot == o.oneShot;
    }
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
    bool addedWhilePlaying = false;  // added by a script (not loaded with the scene): MultiplayerSpawner copies these
    std::vector<SavedConnection> connections;  // this node's signals, connected in the editor

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

    // Signals (see signals.hpp; scripts usually use Script::connect / emitSignal)
    // The signals this kind of node sends by itself (Area2D: body_entered...)
    virtual void getSignals(std::vector<SignalInfo>& out) const;
    std::vector<SignalInfo> signalList() const;  // those, plus the ones its script declares
    bool hasSignal(const std::string& name) const;
    // Sends a signal: calls everything connected to it, in the order they connected.
    template <class... A> void emitSignal(const std::string& name, A&&... args);
    void emitSignalArgs(const std::string& name, const SignalArgs& args);
    bool isSignalConnected(const std::string& name) const { return hub_ && hub_->connected(name); }
    std::shared_ptr<SignalHub> signalHub();  // (the connections; made when first needed)
    std::weak_ptr<void> lifetime() const { return life_; }  // expires when the node is freed

    // Multiplayer (see multiplayer.hpp): the peer whose copy of this node is the
    // real one. 1 (the server) unless set. Not saved in scene files.
    void setMultiplayerAuthority(int peerId, bool recursive = true);
    int multiplayerAuthority() const { return authority_; }
    bool isMultiplayerAuthority() const;  // this game is the authority (always true when not networked)

private:
    friend class SceneTree;
    std::shared_ptr<SignalHub> hub_;
    std::shared_ptr<char> life_ = std::make_shared<char>();
    int authority_ = 1;
    Node* parent_ = nullptr;
    std::vector<std::unique_ptr<Node>> children_;
    SceneTree* tree_ = nullptr;  // only set on the root node
    bool queuedForDeletion_ = false;
};

// ---------------------------------------------------------------------------
// The value of one of a node's shader parameters (see shaders.hpp); the
// field that's used depends on the uniform's type.
struct ShaderParam {
    float f = 0;
    int i = 0;
    bool b = false;
    raylib::Vector2 v{0, 0};
    raylib::Color c{255, 255, 255, 255};  // vec3 / vec4
    std::string texture;                  // sampler2D: a picture in the project
};

class Node2D : public Node {
public:
    raylib::Vector2 position{0, 0};
    float rotation = 0;  // degrees, clockwise (y points down, like Godot)
    raylib::Vector2 scale{1, 1};
    bool visible = true;

    // A custom shader (a .shader file, e.g. "shaders/wave.shader") that draws
    // this node, and its parameters' values (Sprite2D, AnimatedSprite2D,
    // ColorRect and Label use it). From a script:
    //     self<Sprite2D>()->setShaderParam("wave_size", 0.05f);
    std::string shader;
    std::map<std::string, ShaderParam> shaderParams;
    void setShaderParam(const std::string& name, float value);
    void setShaderParam(const std::string& name, double value) { setShaderParam(name, static_cast<float>(value)); }
    void setShaderParam(const std::string& name, int value);
    void setShaderParam(const std::string& name, bool value);
    void setShaderParam(const std::string& name, ::Vector2 value);
    void setShaderParam(const std::string& name, ::Color value);
    void setShaderTexture(const std::string& name, const std::string& texturePath);

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
    // True for a CanvasLayer: it starts a space of its own (the screen), so the
    // nodes above it don't move it.
    virtual bool startsOwnSpace() const { return false; }

protected:
    // "shader" and its parameters, for the nodes that draw something
    void addShaderProperties(std::vector<Property>& out);
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
    void getSignals(std::vector<SignalInfo>& out) const override;  // animation_finished, animation_looped, frame_changed
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

    // Signals: body_entered(Node* body), body_exited(Node* body),
    // area_entered(Node* area), area_exited(Node* area)
    void getSignals(std::vector<SignalInfo>& out) const override;
    void update(float delta) override;  // (checks what came in and went out)
    void nodesLeaving(const std::vector<Node*>& leaving);  // (they're being freed: they went out)

private:
    struct Inside {
        Node* node;
        std::weak_ptr<void> life;
    };
    std::vector<Inside> bodiesIn_, areasIn_;
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
    // (Used by multiplayer prediction, to put the body back as the server had it.)
    void setContacts(bool floor, bool wall, bool ceiling) {
        onFloor_ = floor;
        onWall_ = wall;
        onCeiling_ = ceiling;
    }

private:
    bool onFloor_ = false, onWall_ = false, onCeiling_ = false;
};

// ---------------------------------------------------------------------------
// Multiplayer nodes (they do their work through multiplayer.cpp)

// Keeps properties of a node the same on every peer: the node's multiplayer
// authority sends them, everyone else takes them. Put it inside the node.
//   properties: "position, velocity, Sprite2D:flip_h, :health"
//   - a property of the root node ("position"), or of a node below it
//     ("Sprite2D:flip_h"), or a script variable (":health" on the root,
//     "Gun:ammo" also works for a script on Gun)
// With interpolate on, numbers, positions and colors glide between updates
// instead of jumping; other values change when their update is reached.
class MultiplayerSynchronizer : public Node {
public:
    std::string rootPath = "..";        // the node whose properties are kept in sync
    std::string properties = "position";
    float interval = 0;                 // seconds between updates (0 = every frame)
    // Smooth movement: the other games show the node a moment in the past
    // (interpolation_delay), blending between the updates around that time.
    bool interpolate = true;
    float interpolationDelay = 0.1f;

    const char* typeName() const override { return "MultiplayerSynchronizer"; }
    void getProperties(std::vector<Property>& out) override;

    double sinceSent = 1e9;             // (used by multiplayer.cpp)
    std::string lastSent;
};

// When the server adds a scene (made with instantiate()) below spawnPath, every
// client gets a copy too, with the same name and multiplayer authority; when
// the server frees it, it goes everywhere. Players who join later get a copy of
// everything spawned so far. Nodes already in the scene file aren't copied.
class MultiplayerSpawner : public Node {
public:
    std::string spawnPath = "..";       // the node the spawned scenes go into

    const char* typeName() const override { return "MultiplayerSpawner"; }
    void getProperties(std::vector<Property>& out) override;
    void getSignals(std::vector<SignalInfo>& out) const override;  // spawned(Node*), despawned(Node*) (on clients)
};

// Counts down wait_time seconds, then sends "timeout". Again and again, or
// once (one_shot). Starts by itself with autostart, or when a script calls start().
//   connect(getNode("Timer"), "timeout", [this] { spawnEnemy(); });
class Timer : public Node {
public:
    float waitTime = 1.0f;
    bool oneShot = false;
    bool autostart = false;

    const char* typeName() const override { return "Timer"; }
    void getProperties(std::vector<Property>& out) override;
    void getSignals(std::vector<SignalInfo>& out) const override;  // timeout
    void update(float delta) override;

    void start(float seconds = -1);  // -1: wait_time
    void stop();
    bool isStopped() const { return !running_; }
    float timeLeft() const { return running_ ? left_ : 0; }

private:
    bool running_ = false, started_ = false;
    float left_ = 0;
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

template <class... A> void Node::emitSignal(const std::string& name, A&&... args) {
    emitSignalArgs(name, SignalArgs{detail::toVariant(std::forward<A>(args))...});
}

// Draws a rectangle given in a node's own space, rotated and scaled with it.
void drawLocalRect(const Node2D& node, Rectangle local, Color fill, Color outline, float lineWidth);

}  // namespace s2d
