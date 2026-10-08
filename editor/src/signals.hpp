// Signals, like Godot's: a node announces that something happened ("the body
// entered", "the timer ran out"), and any script that connected to it gets a
// call. The node doesn't need to know who's listening.
//
//     // In a script: listen to a node's signal
//     connect(getNode("Area2D"), "body_entered", &Coin::onBodyEntered);
//     connect(getNode("Timer"), "timeout", [this] { spawnEnemy(); });
//
//     // Declare and send your own (see SIGNAL in script.hpp)
//     SIGNAL(health_changed, int);
//     emitSignal("health_changed", health);
//
// A connection is undone by itself when the script that made it goes away
// (its node is freed, the scene changes, the script is hot reloaded), so a
// signal never calls a script that's gone.
#pragma once

#include <raylib-cpp.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <string>
#include <variant>
#include <vector>

namespace s2d {

class Node;

// One value sent with a signal: what EXPORT shows, plus a node.
using Variant = std::variant<std::monostate, bool, int, float, std::string, Vector2, Color, Node*>;
using SignalArgs = std::vector<Variant>;

namespace detail {
// A value as a Variant: doubles become floats, other whole numbers ints, any
// node pointer a Node*.
template <class T> Variant toVariant(T&& v) {
    using D = std::decay_t<T>;
    if constexpr (std::is_same_v<D, bool>) return Variant(v);
    else if constexpr (std::is_floating_point_v<D>) return Variant(static_cast<float>(v));
    else if constexpr (std::is_integral_v<D> || std::is_enum_v<D>) return Variant(static_cast<int>(v));
    else if constexpr (std::is_base_of_v<::Vector2, D>) return Variant(::Vector2{v.x, v.y});
    else if constexpr (std::is_base_of_v<::Color, D>) return Variant(::Color{v.r, v.g, v.b, v.a});
    else if constexpr (std::is_pointer_v<D> && std::is_base_of_v<Node, std::remove_cv_t<std::remove_pointer_t<D>>>)
        return Variant(static_cast<Node*>(const_cast<std::remove_cv_t<std::remove_pointer_t<D>>*>(v)));
    else if constexpr (std::is_convertible_v<D, std::string>) return Variant(std::string(v));
    else return Variant(std::forward<T>(v));
}
// A Variant as the type a connected function wants (numbers convert; a node
// pointer becomes the node type asked for, or nullptr if it isn't one)
Node* variantNode(const Variant& v);
template <class T, class V> struct holds;
template <class T, class... Ts> struct holds<T, std::variant<Ts...>> : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};
template <class T> T fromVariant(const Variant& v) {
    if constexpr (std::is_pointer_v<T>) {
        using N = std::remove_cv_t<std::remove_pointer_t<T>>;
        if constexpr (std::is_same_v<N, Node>) return variantNode(v);
        else return dynamic_cast<T>(variantNode(v));
    } else if constexpr (std::is_same_v<T, raylib::Vector2>) {
        auto* p = std::get_if<Vector2>(&v);
        return p ? raylib::Vector2(*p) : raylib::Vector2{0, 0};
    } else if constexpr (std::is_same_v<T, raylib::Color>) {
        auto* p = std::get_if<Color>(&v);
        return p ? raylib::Color(*p) : raylib::Color{0, 0, 0, 0};
    } else {
        if constexpr (holds<T, Variant>::value)
            if (auto* p = std::get_if<T>(&v)) return *p;
        if constexpr (std::is_arithmetic_v<T>) {
            if (auto* i = std::get_if<int>(&v)) return static_cast<T>(*i);
            if (auto* f = std::get_if<float>(&v)) return static_cast<T>(*f);
            if (auto* b = std::get_if<bool>(&v)) return static_cast<T>(*b);
        }
        return T{};
    }
}
}  // namespace detail

// A signal a node can send, for the inspector and for checking names.
struct SignalInfo {
    std::string name;
    std::string args;  // e.g. "Node* body", "" for none
};

// The connections to one node's signals (shared, so a script can tell
// whether the node is still there when it disconnects).
struct SignalHub {
    struct Connection {
        std::size_t id = 0;
        std::string signal;
        std::function<void(const SignalArgs&)> call;
        std::weak_ptr<void> owner;  // the listening script; the call is dropped when it's gone
        bool hasOwner = false;
        bool oneShot = false;  // disconnect after the first call
    };
    std::vector<Connection> connections;
    std::size_t nextId = 1;

    std::size_t add(const std::string& signal, std::function<void(const SignalArgs&)> call, std::weak_ptr<void> owner,
                    bool hasOwner, bool oneShot);
    void remove(std::size_t id);
    void emit(const std::string& signal, const SignalArgs& args);
    bool connected(const std::string& signal) const;
};

}  // namespace s2d
