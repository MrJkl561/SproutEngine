#include "sprout/physics.hpp"

#include <algorithm>
#include <cmath>

#include "sprout/tilemap.hpp"

namespace sprout {

namespace {

// Like Rect::overlaps, but ignores overlaps thinner than a tiny amount. Without
// this, rounding errors (320.00003 instead of 320) make a box standing on the
// floor "hit" the floor tiles next to it and get stuck.
constexpr float kSkin = 0.001f;

bool overlapsBy(const Rect& a, const Rect& b) {
    return a.left() < b.right() - kSkin && b.left() < a.right() - kSkin && a.top() < b.bottom() - kSkin &&
           b.top() < a.bottom() - kSkin;
}

// Moves along ONE axis and pushes the box back out of anything it ran into.
void moveAxis(Rect& box, float amount, bool horizontal, const std::vector<Rect>& solids, MoveResult& result) {
    if (amount == 0.0f) return;
    const Rect before = box;
    if (horizontal) box.x += amount;
    else box.y += amount;

    for (const Rect& solid : solids) {
        // Already inside this solid before moving (e.g. spawned there)? Let the box
        // move out of it, instead of pushing it out the far side.
        if (overlapsBy(before, solid)) continue;
        if (!overlapsBy(box, solid)) continue;
        if (horizontal) {
            if (amount > 0.0f) { box.x = solid.left() - box.width; result.hitRight = true; }
            else { box.x = solid.right(); result.hitLeft = true; }
        } else {
            if (amount > 0.0f) { box.y = solid.top() - box.height; result.hitBottom = true; }
            else { box.y = solid.bottom(); result.hitTop = true; }
        }
    }
}

// Splits a big move into small steps so fast things can't skip through thin walls.
template <class GetSolids>
MoveResult moveInSteps(Rect& box, Vec2 motion, GetSolids&& getSolids) {
    MoveResult result;
    const float biggest = std::max(std::abs(motion.x), std::abs(motion.y));
    const float maxStep = std::max(1.0f, std::min(box.width, box.height) / 2.0f);
    const int steps = std::max(1, static_cast<int>(std::ceil(biggest / maxStep)));
    const Vec2 step = motion / static_cast<float>(steps);

    for (int i = 0; i < steps; ++i) {
        const Rect area = Rect{box.x - std::abs(step.x) - 1.0f, box.y - std::abs(step.y) - 1.0f,
                               box.width + std::abs(step.x) * 2.0f + 2.0f, box.height + std::abs(step.y) * 2.0f + 2.0f};
        const std::vector<Rect> solids = getSolids(area);
        MoveResult stepResult;
        if (!result.hitLeft && !result.hitRight) moveAxis(box, step.x, true, solids, stepResult);
        if (!result.hitTop && !result.hitBottom) moveAxis(box, step.y, false, solids, stepResult);
        result.hitLeft |= stepResult.hitLeft;
        result.hitRight |= stepResult.hitRight;
        result.hitTop |= stepResult.hitTop;
        result.hitBottom |= stepResult.hitBottom;
    }
    return result;
}

} // namespace

MoveResult moveAndCollide(Rect& box, Vec2 motion, const std::vector<Rect>& solids) {
    return moveInSteps(box, motion, [&](const Rect& area) {
        std::vector<Rect> nearby;
        for (const Rect& solid : solids)
            if (solid.overlaps(area)) nearby.push_back(solid);
        return nearby;
    });
}

MoveResult moveAndCollide(Rect& box, Vec2 motion, const TileMap& map) {
    return moveInSteps(box, motion, [&](const Rect& area) { return map.solidRectsIn(area); });
}

} // namespace sprout
