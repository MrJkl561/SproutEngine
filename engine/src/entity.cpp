#include "sprout/entity.hpp"

#include <cmath>
#include <stdexcept>

#include "sprout/scene.hpp"
#include "sprout/tilemap.hpp"

namespace sprout {

Rect Entity::bounds() const { return Rect::fromCenter(position, size); }

bool Entity::overlaps(const Entity& other) const { return bounds().overlaps(other.bounds()); }

bool Entity::overlaps(const Rect& area) const { return bounds().overlaps(area); }

Scene& Entity::scene() const {
    if (!scene_) {
        throw std::logic_error(
            "Entity::scene() was used before the entity was added to a scene. If you're in the entity's "
            "constructor, move that code into start() instead.");
    }
    return *scene_;
}

int Entity::after(float seconds, std::function<void()> action) {
    const int id = scene().after(seconds, [this, action = std::move(action)] {
        if (!isDestroyed()) action();
    });
    timers_.push_back(id);
    return id;
}

int Entity::every(float seconds, std::function<void()> action) {
    const int id = scene().every(seconds, [this, action = std::move(action)] {
        if (!isDestroyed()) action();
    });
    timers_.push_back(id);
    return id;
}

MoveResult Entity::moveAndSlide(float dt) {
    Rect box = bounds();
    const Vec2 motion = velocity * dt;

    // Gather everything solid that the entity could touch on the way.
    std::vector<Rect> solids;
    if (scene_) {
        const Rect area = Rect{box.x - std::abs(motion.x) - 1.0f, box.y - std::abs(motion.y) - 1.0f,
                               box.width + std::abs(motion.x) * 2.0f + 2.0f,
                               box.height + std::abs(motion.y) * 2.0f + 2.0f};
        for (const Rect& rect : scene_->solidRects())
            if (rect.overlaps(area)) solids.push_back(rect);
        for (const TileMap* map : scene_->solidMaps()) {
            std::vector<Rect> tiles = map->solidRectsIn(area);
            solids.insert(solids.end(), tiles.begin(), tiles.end());
        }
    }

    lastMove_ = moveAndCollide(box, motion, solids);
    position = box.center();
    if (lastMove_.hitWall()) velocity.x = 0.0f;
    if ((lastMove_.hitBottom && velocity.y > 0.0f) || (lastMove_.hitTop && velocity.y < 0.0f)) velocity.y = 0.0f;
    return lastMove_;
}

} // namespace sprout
