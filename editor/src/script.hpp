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
#include <type_traits>
#include <vector>

#include "nodes.hpp"
#include "scene_tree.hpp"

namespace s2d {

// The variables a script shows in the inspector (like Godot's @export). The
// easy way is EXPORT, which declares the variable and shows it in one go (see
// the end of this file):
//
//     EXPORT(float, speed, 250);
//     EXPORT_SCENE(nextLevel);
//
// Or list them yourself in the script's exports(), for full control:
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
    // Everything the inspector shows: the EXPORT variables, then exports().
    void collectExports(Exports& e) {
        for (auto& add : autoExports_) add(e);
        exports(e);
    }
    // (Used by EXPORT.)
    void addAutoExport(std::function<void(Exports&)> add) { autoExports_.push_back(std::move(add)); }

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
    std::vector<std::function<void(Exports&)>> autoExports_;
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

// ---------------------------------------------------------------------------
// EXPORT: a variable that shows in the inspector, in one line (like Godot's
// @export). Inside your script class:
//
//     EXPORT(float, speed, 250);                 // float speed = 250, shown as "speed"
//     EXPORT(int, lives, 3);
//     EXPORT(bool, canDoubleJump, true);         // shown as "can_double_jump"
//     EXPORT(std::string, greeting, "Hello");
//     EXPORT(Vector2, launch, 0, -400);          // several values: Vector2{0, -400}
//     EXPORT(Color, tint, 255, 200, 200, 255);
//     EXPORT_STEP(float, gravity, 1400, 5);      // dragging the number changes it by 5
//     EXPORT_SCENE(nextLevel, "scenes/level2.scene.json");  // picked from the scenes
//     EXPORT_TEXTURE(skin, "assets/player.png");             // picked from the pictures
//
// Use them like any other variable (speed * delta). Names in the inspector
// (and the scene files) are the variable's name in snake_case.
namespace s2d::detail {
// "jumpSpeed" -> "jump_speed"
inline std::string exportName(const char* name) {
    std::string out;
    for (const char* c = name; *c; ++c) {
        if (*c >= 'A' && *c <= 'Z') {
            if (!out.empty() && out.back() != '_') out += '_';
            out += static_cast<char>(*c - 'A' + 'a');
        } else {
            out += *c;
        }
    }
    return out;
}
inline void addExport(Exports& e, const std::string& n, float& v, float step) { e.add(n, v, step); }
inline void addExport(Exports& e, const std::string& n, int& v, float) { e.add(n, v); }
inline void addExport(Exports& e, const std::string& n, bool& v, float) { e.add(n, v); }
inline void addExport(Exports& e, const std::string& n, std::string& v, float) { e.add(n, v); }
inline void addExport(Exports& e, const std::string& n, Vector2& v, float step) { e.add(n, v, step); }
inline void addExport(Exports& e, const std::string& n, raylib::Vector2& v, float step) { e.add(n, v, step); }
inline void addExport(Exports& e, const std::string& n, Color& v, float) { e.add(n, v); }
inline void addExport(Exports& e, const std::string& n, raylib::Color& v, float) { e.add(n, v); }
template <class T> void addExport(Exports&, const std::string&, T&, float) {
    static_assert(sizeof(T) == 0, "EXPORT works with float, int, bool, std::string, Vector2 and Color");
}

// The types EXPORT can show in the inspector
template <class T>
inline constexpr bool exportable =
    std::is_same_v<T, float> || std::is_same_v<T, int> || std::is_same_v<T, bool> || std::is_same_v<T, std::string> ||
    std::is_same_v<T, Vector2> || std::is_same_v<T, raylib::Vector2> || std::is_same_v<T, Color> ||
    std::is_same_v<T, raylib::Color>;

enum class ExportAs { Value, Scene, Texture };
// Lives next to the variable and puts it in the inspector's list.
template <class T> class ExportField {
public:
    ExportField(Script* script, const char* name, T& value, float step = 1.0f, ExportAs as = ExportAs::Value) {
        script->addAutoExport([key = exportName(name), &value, step, as](Exports& e) {
            if constexpr (std::is_same_v<T, std::string>) {
                if (as == ExportAs::Scene) return e.scene(key, value);
                if (as == ExportAs::Texture) return e.texture(key, value);
            }
            addExport(e, key, value, step);
        });
    }
    ExportField(const ExportField&) = delete;  // (the list points at this script's own variables)
    ExportField& operator=(const ExportField&) = delete;
};
}  // namespace s2d::detail

#define EXPORT(Type, name, ...)                                                                    \
    static_assert(::s2d::detail::exportable<Type>,                                                 \
                  "EXPORT works with float, int, bool, std::string, Vector2 and Color");          \
    Type name{__VA_ARGS__};                                                                        \
    ::s2d::detail::ExportField<Type> s2dExport_##name { this, #name, name }
#define EXPORT_STEP(Type, name, value, step)                                                       \
    static_assert(::s2d::detail::exportable<Type>,                                                 \
                  "EXPORT works with float, int, bool, std::string, Vector2 and Color");          \
    Type name{value};                                                                              \
    ::s2d::detail::ExportField<Type> s2dExport_##name { this, #name, name, static_cast<float>(step) }
#define EXPORT_SCENE(name, ...)     \
    std::string name{__VA_ARGS__}; \
    ::s2d::detail::ExportField<std::string> s2dExport_##name { this, #name, name, 1.0f, ::s2d::detail::ExportAs::Scene }
#define EXPORT_TEXTURE(name, ...)   \
    std::string name{__VA_ARGS__}; \
    ::s2d::detail::ExportField<std::string> s2dExport_##name { this, #name, name, 1.0f, ::s2d::detail::ExportAs::Texture }

// Makes a script class show up in the editor. Put it after the class.
#define REGISTER_SCRIPT(ClassName)                                            \
    static const bool s2dRegistered_##ClassName = ::s2d::ScriptRegistry::add( \
        #ClassName, [] { return std::unique_ptr<::s2d::Script>(new ClassName()); });
