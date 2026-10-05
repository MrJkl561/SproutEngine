// C++ scripts. A script is a class that is attached to a node and gets
// ready() once when the game starts and process(delta) every frame, just like
// _ready() and _process() in Godot.
//
// Put your scripts in the editor/scripts/ folder (or press "New" under Script
// in the inspector). The editor compiles and reloads them every time you save
// one, even while the game is running; they show up in the inspector's
// script menu:
//
//     #include "script.hpp"
//     using namespace s2d;
//
//     class Spinner : public Script {
//     public:
//         float speed = 90;
//         void process(float delta) override { self<Node2D>()->rotation += speed * delta; }
//     };
//     REGISTER_SCRIPT(Spinner)
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "nodes.hpp"
#include "scene_tree.hpp"

namespace s2d {

// The variables a script shows in the inspector (like Godot's @export).
// List them in your script's exports():
//
//     float speed = 250;
//     std::string nextLevel;
//     void exports(Exports& e) override {
//         e.add("speed", speed);
//         e.scene("next_level", nextLevel);
//     }
//
// The values set in the inspector are put into the variables before ready().
class Exports {
public:
    void add(const std::string& name, float& value, float dragStep = 1.0f) {
        list.push_back({name, PropKind::Float, &value, dragStep});
    }
    void add(const std::string& name, int& value) { list.push_back({name, PropKind::Int, &value, 0.2f}); }
    void add(const std::string& name, bool& value) { list.push_back({name, PropKind::Bool, &value}); }
    void add(const std::string& name, std::string& value) { list.push_back({name, PropKind::String, &value}); }
    void add(const std::string& name, Vector2& value, float dragStep = 1.0f) {
        list.push_back({name, PropKind::Vec2, &value, dragStep});
    }
    void add(const std::string& name, Color& value) { list.push_back({name, PropKind::Color, &value}); }
    // A scene file, picked from a list ("scenes/level2.scene.json").
    void scene(const std::string& name, std::string& path) { list.push_back({name, PropKind::Scene, &path}); }
    // A picture, picked from a list ("assets/coin.png").
    void texture(const std::string& name, std::string& path) { list.push_back({name, PropKind::Texture, &path}); }

    std::vector<Property> list;
};

class Script {
public:
    virtual ~Script() = default;

    virtual void ready() {}
    virtual void process(float /*delta*/) {}
    // The variables to show in the inspector; see Exports above.
    virtual void exports(Exports& /*e*/) {}

    // The node this script is attached to.
    Node* node() const { return node_; }
    // The node as a particular type, e.g. self<CharacterBody2D>(); nullptr if it isn't one.
    template <class T> T* self() const { return node_ ? dynamic_cast<T*>(node_) : nullptr; }

    // Finds a node by path from this one: "Child", "../Sibling".
    Node* getNode(const std::string& path) const { return node_ ? node_->getNode(path) : nullptr; }
    // Finds a node by name anywhere in the scene.
    Node* findNode(const std::string& name) const;
    template <class T> T* findNode(const std::string& name) const {
        Node* n = findNode(name);
        return n ? dynamic_cast<T*>(n) : nullptr;
    }

    SceneTree* tree() const { return node_ ? node_->tree() : nullptr; }

    // Switch to another scene at the end of this frame, or restart this one.
    void changeScene(const std::string& scenePath) const {
        if (SceneTree* t = tree()) t->changeScene(scenePath);
    }
    void reloadScene() const {
        if (SceneTree* t = tree()) t->reloadScene();
    }

private:
    friend class SceneTree;
    Node* node_ = nullptr;
};

// Makes a new copy of a scene file, to add with addChild(), e.g.
//   node()->addChild(instantiate("scenes/coin.scene.json"));
// Returns nullptr if the file can't be loaded.
std::unique_ptr<Node> instantiate(const std::string& scenePath);

// Writes a line to the editor's Output panel.
void print(const std::string& message);
// Where print() sends its text (the editor points this at its Output panel).
void setPrintHandler(std::function<void(const std::string&)> handler);

class ScriptRegistry {
public:
    using Factory = std::function<std::unique_ptr<Script>()>;
    static bool add(const std::string& name, Factory factory);
    static void remove(const std::string& name);
    static std::unique_ptr<Script> create(const std::string& name);
    static bool has(const std::string& name);
    static std::vector<std::string> names();  // sorted
};

}  // namespace s2d

// Makes a script class show up in the editor. Put it after the class.
#define REGISTER_SCRIPT(ClassName)                                            \
    static const bool s2dRegistered_##ClassName = ::s2d::ScriptRegistry::add( \
        #ClassName, [] { return std::unique_ptr<::s2d::Script>(new ClassName()); });
