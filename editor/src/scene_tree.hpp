// Owns the root node of a scene and runs it: attaches scripts, calls
// ready()/process(), frees nodes that called queueFree() and draws everything.
#pragma once

#include <functional>
#include <memory>
#include <string>

#include "nodes.hpp"

namespace s2d {

class SceneTree {
public:
    SceneTree() = default;
    ~SceneTree();
    SceneTree(const SceneTree&) = delete;
    SceneTree& operator=(const SceneTree&) = delete;

    Node* root() const { return root_.get(); }
    void setRoot(std::unique_ptr<Node> root);
    std::unique_ptr<Node> takeRoot();

    // Game loop
    void start();                // creates every node's script and calls ready()
    void process(float delta);   // process() on every script, then removes freed nodes
    void draw(const DrawOptions& options);
    bool isPlaying() const { return playing_; }
    float delta() const { return delta_; }

    // Hot reloading: destroy every node's script, then make them again (and
    // call ready()) from whatever scripts are registered now.
    void dropScripts();
    void restartScripts();

    // The Camera2D the game looks through (the first one marked "current").
    Camera2D* currentCamera() const;

    // Called by Node::addChild so nodes added while playing get their scripts.
    void nodeAdded(Node* node);

    raylib::Vector2 gameSize{800, 450};  // the size of the game window
    raylib::Color clearColor{40, 44, 52, 255};

private:
    void attachScripts(Node* node);
    void readyRecursive(Node* node);

    std::unique_ptr<Node> root_;
    bool playing_ = false;
    float delta_ = 1.0f / 60.0f;
};

// Calls fn for node and everything below it, parents before children.
void forEachNode(Node* node, const std::function<void(Node*)>& fn);

}  // namespace s2d
