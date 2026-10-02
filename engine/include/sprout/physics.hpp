#pragma once
// =============================================================================
//  sprout/physics.hpp - moving boxes without going through walls
//
//  Most of the time you'll just call moveAndSlide(dt) inside an Entity
//  (see entity.hpp). These functions are what it uses underneath, and you can
//  use them yourself for anything that is a Rect.
//
//    Rect box{100, 100, 32, 32};
//    MoveResult hit = moveAndCollide(box, velocity * dt, walls);
//    if (hit.hitBottom) { /* landed on something */ }
// =============================================================================
#include <vector>

#include "sprout/math.hpp"

namespace sprout {

class TileMap;

/// What a box bumped into while moving.
struct MoveResult {
    bool hitLeft = false;   ///< bumped into something on its left
    bool hitRight = false;  ///< bumped into something on its right
    bool hitTop = false;    ///< bumped its head on a ceiling
    bool hitBottom = false; ///< landed on a floor

    bool hitAnything() const { return hitLeft || hitRight || hitTop || hitBottom; }
    bool hitWall() const { return hitLeft || hitRight; }
};

/// Moves `box` by `motion`, stopping it at any of the `solids`.
/// The box slides along walls instead of getting stuck.
MoveResult moveAndCollide(Rect& box, Vec2 motion, const std::vector<Rect>& solids);
/// Same, but the solid things are the solid tiles of a tile map.
MoveResult moveAndCollide(Rect& box, Vec2 motion, const TileMap& map);

} // namespace sprout
