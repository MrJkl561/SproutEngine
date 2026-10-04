#include "scene_tree.hpp"

#include <map>
#include <vector>

#include "script.hpp"

namespace s2d {

void forEachNode(Node* node, const std::function<void(Node*)>& fn) {
    if (!node) return;
    fn(node);
    // Index loop: fn may add children while we walk.
    for (size_t i = 0; i < node->children().size(); ++i) forEachNode(node->children()[i].get(), fn);
}

SceneTree::~SceneTree() {
    // Scripts may look at the tree in their destructors, so free nodes first.
    root_.reset();
}

void SceneTree::setRoot(std::unique_ptr<Node> root) {
    if (root_) root_->tree_ = nullptr;
    root_ = std::move(root);
    if (root_) root_->tree_ = this;
}

std::unique_ptr<Node> SceneTree::takeRoot() {
    if (root_) root_->tree_ = nullptr;
    return std::move(root_);
}

void SceneTree::attachScripts(Node* node) {
    forEachNode(node, [](Node* n) {
        if (n->scriptName.empty() || n->script) return;
        n->script = ScriptRegistry::create(n->scriptName);
        if (n->script) {
            n->script->node_ = n;
        } else {
            print("Script '" + n->scriptName + "' on node '" + n->name +
                  "' isn't compiled into the editor yet. Rebuild the editor to use it.");
        }
    });
}

void SceneTree::readyRecursive(Node* node) {
    // Children are ready before their parent, like in Godot.
    for (size_t i = 0; i < node->children().size(); ++i) readyRecursive(node->children()[i].get());
    if (node->script) node->script->ready();
}

void SceneTree::start() {
    playing_ = true;
    if (!root_) return;
    attachScripts(root_.get());
    readyRecursive(root_.get());
}

void SceneTree::nodeAdded(Node* node) {
    if (!playing_) return;
    attachScripts(node);
    readyRecursive(node);
}

void SceneTree::process(float delta) {
    delta_ = delta;
    if (!root_) return;

    // Collect first, so nodes added during this frame start next frame.
    std::vector<Node*> nodes;
    forEachNode(root_.get(), [&](Node* n) { nodes.push_back(n); });
    for (Node* n : nodes)
        if (n->script && !n->isQueuedForDeletion()) n->script->process(delta);

    // Free everything that called queueFree() (and its children with it).
    std::function<void(Node*)> sweep = [&](Node* n) {
        auto& kids = n->children();
        for (size_t i = 0; i < kids.size();) {
            if (kids[i]->isQueuedForDeletion()) {
                kids[i]->detach();  // the returned unique_ptr frees it
            } else {
                sweep(kids[i].get());
                ++i;
            }
        }
    };
    sweep(root_.get());
}

void SceneTree::draw(const DrawOptions& options) {
    std::function<void(Node*)> drawNode = [&](Node* n) {
        if (auto* n2d = dynamic_cast<Node2D*>(n); n2d && !n2d->visible) return;  // hides children too
        n->draw(options);
        for (auto& c : n->children()) drawNode(c.get());
    };
    if (root_) drawNode(root_.get());
}

Camera2D* SceneTree::currentCamera() const {
    Camera2D* found = nullptr;
    forEachNode(root_.get(), [&](Node* n) {
        if (auto* cam = dynamic_cast<Camera2D*>(n); cam && cam->current && !found) found = cam;
    });
    return found;
}

// ---------------------------------------------------------------------------
// Script registry
// ---------------------------------------------------------------------------
namespace {

std::map<std::string, ScriptRegistry::Factory>& registry() {
    static std::map<std::string, ScriptRegistry::Factory> map;  // made on first use, so static init order is safe
    return map;
}

}  // namespace

bool ScriptRegistry::add(const std::string& name, Factory factory) {
    registry()[name] = std::move(factory);
    return true;
}

std::unique_ptr<Script> ScriptRegistry::create(const std::string& name) {
    auto it = registry().find(name);
    return it == registry().end() ? nullptr : it->second();
}

bool ScriptRegistry::has(const std::string& name) { return registry().count(name) > 0; }

std::vector<std::string> ScriptRegistry::names() {
    std::vector<std::string> out;
    for (auto& [name, f] : registry()) out.push_back(name);
    return out;
}

namespace {
std::function<void(const std::string&)>& printHandler() {
    static std::function<void(const std::string&)> handler;
    return handler;
}
}  // namespace

void print(const std::string& message) {
    if (printHandler()) printHandler()(message);
    else TraceLog(LOG_INFO, "%s", message.c_str());
}

void setPrintHandler(std::function<void(const std::string&)> handler) { printHandler() = std::move(handler); }

Node* Script::findNode(const std::string& name) const { return node_ ? node_->root()->findNode(name) : nullptr; }

}  // namespace s2d
