// Owns the root node of a scene and runs it: attaches scripts, calls
// ready()/process(), frees nodes that called queueFree() and draws everything.
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "nodes.hpp"
#include "project.hpp"

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
    // Draws the scene. In the game (options.editor false) the CanvasLayers are
    // left out: draw them with drawCanvasLayers() after the camera is done.
    void draw(const DrawOptions& options);
    void drawCanvasLayers(const DrawOptions& options);
    bool isPlaying() const { return playing_; }
    float delta() const { return delta_; }

    // Changing scenes while the game runs (like Godot's change_scene_to_file).
    // The switch happens at the end of the current frame, so a script can call
    // it on its own scene: every node and script of the old scene is freed,
    // then the new scene's scripts get ready(). Paths are inside the project,
    // e.g. "scenes/level2.scene.json".
    void changeScene(const std::string& scenePath);
    void reloadScene();  // start the current scene again (e.g. after the player dies)
    // Ends the game after this frame (in the editor: stops playing).
    void quit() { quitRequested_ = true; }
    bool quitRequested() const { return quitRequested_; }
    const std::string& currentScene() const { return currentScene_; }
    void setCurrentScene(const std::string& scenePath) { currentScene_ = scenePath; }

    // Autoloads (gear button > Autoloads..., saved in project.json): nodes made
    // before the first scene and kept until the game stops, whatever scene is
    // running. They sit beside the scene, not above it, so the scene's node
    // paths don't change. Call before start(); they get ready() first.
    void setAutoloads(const std::vector<Autoload>& autoloads);
    Node* autoload(const std::string& name) const;  // nullptr if there's none by that name
    const std::vector<std::unique_ptr<Node>>& autoloads() const { return autoloads_; }

    // Values kept while the game runs, across scene changes (score, lives...):
    //   tree()->values["coins"] += 1;
    std::map<std::string, double> values;

    // Hot reloading: destroy every node's script, then make them again (and
    // call ready()) from whatever scripts are registered now.
    void dropScripts();
    void restartScripts();

    // The Camera2D the game looks through (the first one marked "current").
    Camera2D* currentCamera() const;
    // The view the game is drawn with: that camera, centred on the screen.
    raylib::Camera2D view() const;

    // The mouse, in game pixels (0,0 = the game window's top-left corner),
    // set every frame by whatever runs the game (the window may be scaled).
    raylib::Vector2 mouseScreen{-100000, -100000};
    raylib::Vector2 mouseWorld() const;  // where it is in the world, through the camera
    bool isMouseOverGui() const { return guiHot.get() != nullptr; }

    // (Used by ui_nodes.cpp: the UI node under the mouse, the one being
    // clicked, and the one being typed in.)
    struct NodeRef {
        Node* get() const { return life.expired() ? nullptr : node; }
        void set(Node* n) {
            node = n;
            life = n ? n->lifetime() : std::weak_ptr<void>();
        }
        Node* node = nullptr;
        std::weak_ptr<void> life;
    };
    NodeRef guiHot, guiActive, guiFocus;
    int testMouse = -1;            // (tests) -1: the real mouse button; 0 up, 1 down
    bool testMouseWasDown = false;

    // Called by Node::addChild so nodes added while playing get their scripts.
    void nodeAdded(Node* node);

    raylib::Vector2 gameSize{800, 450};  // the size of the game window
    raylib::Color clearColor{40, 44, 52, 255};

private:
    void attachScripts(Node* node);
    void connectSaved(Node* node);  // the signal connections made in the editor, below node
    void readyRecursive(Node* node);

    void switchScene();

    std::unique_ptr<Node> root_;
    std::vector<std::unique_ptr<Node>> autoloads_;
    std::string currentScene_;
    std::string pendingScene_;  // set by changeScene(), done at the end of process()
    bool playing_ = false;
    bool quitRequested_ = false;
    float delta_ = 1.0f / 60.0f;
};

// Calls fn for node and everything below it, parents before children.
void forEachNode(Node* node, const std::function<void(Node*)>& fn);

}  // namespace s2d
