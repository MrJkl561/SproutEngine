#include "script.hpp"

#include <functional>

using namespace s2d;

// An autoload: prints what the scene is made of, so the test can check what
// importing a Tiled map made.
class Checker : public Script {
public:
    void ready() override {
        Node* root = tree()->root();
        std::function<void(Node*, const std::string&)> walk = [&](Node* n, const std::string& path) {
            std::string line = "NODE " + path + " " + n->typeName();
            if (auto* n2 = n->as<Node2D>())
                line += " at=" + std::to_string((int)n2->position.x) + "," + std::to_string((int)n2->position.y) +
                        (n2->visible ? "" : " hidden");
            if (auto* map = n->as<TileMap>()) {
                line += " cells=" + std::to_string(map->cells.size());
                for (auto& s : map->sources) {
                    line += " src=" + s.texture + " solid=";
                    for (int t : s.solid) line += std::to_string(t) + ";";
                    for (auto& [t, name] : s.types) line += " type" + std::to_string(t) + "=" + name;
                }
                line += " alpha=" + std::to_string(map->modulate.a) + " list=";
                for (auto& [pos, c] : map->cells)
                    line += std::to_string(pos.first) + "," + std::to_string(pos.second) + ":" + std::to_string(c.source) +
                            "/" + std::to_string(c.tile) + "/" + std::to_string(c.flags) + " ";
            }
            if (auto* shape = n->as<CollisionShape2D>())
                line += " size=" + std::to_string((int)shape->size.x) + "x" + std::to_string((int)shape->size.y);
            if (auto* sprite = n->as<Sprite2D>())
                line += " tex=" + sprite->texture + " frame=" + std::to_string(sprite->frame);
            if (auto* label = n->as<Label>()) line += " text=" + label->text;
            print(line);
            for (auto& c : n->children()) walk(c.get(), path + "/" + c->name);
        };
        if (root) walk(root, root->name);
        print("CHECKED");
    }
};

REGISTER_SCRIPT(Checker)
