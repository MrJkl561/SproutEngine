// Imports maps made in Tiled (https://www.mapeditor.org): .tmx (XML) and .tmj
// (JSON) files, with their tile sets inside them or in .tsx / .tsj files.
//
// A map becomes a Node2D named after the file, with a node for each layer:
//   - a tile layer -> a TileMap (tiles with a "solid", "collision" or
//     "collides" property, or collision shapes drawn in Tiled's tile
//     collision editor, collide; a tile's class/type becomes its type name)
//   - an object layer -> a Node2D with a node for each object: rectangles
//     become Area2Ds (StaticBody2Ds if the object or layer is called/classed
//     "solid", "collision" or "wall"), tile objects Sprite2Ds, points and other
//     shapes plain Node2Ds marking the spot
//   - an image layer -> a Sprite2D
//   - a group -> a Node2D with its layers inside
// Pictures outside the project are copied into the project's assets/tiles/.
// Only orthogonal maps; tile sets made of one picture each.
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "nodes.hpp"

namespace s2d {

struct TiledImport {
    std::unique_ptr<Node> root;          // nullptr if the map couldn't be read
    std::string error;                   // why not
    std::vector<std::string> warnings;   // things that were skipped
    std::vector<std::string> copied;     // pictures copied into the project (project paths)
};

TiledImport importTiledMap(const std::filesystem::path& mapFile, const std::filesystem::path& projectDir);

// .tmx or .tmj
bool isTiledMap(const std::filesystem::path& file);

}  // namespace s2d
