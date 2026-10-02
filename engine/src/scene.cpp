#include "sprout/scene.hpp"

#include <algorithm>
#include <utility>

#include "internal.hpp"
#include "sprout/game.hpp"
#include "sprout/tilemap.hpp"

namespace sprout {

Scene::Scene() { camera.reset(); }

Scene::~Scene() = default;

// --- Entities ----------------------------------------------------------------------
Entity& Scene::addEntity(std::unique_ptr<Entity> entity) {
    Entity& result = *entity;
    result.scene_ = this;
    spawned_.push_back(std::move(entity));
    result.start();
    return result;
}

void Scene::mergeSpawned() {
    // Moving unique_ptrs doesn't move the entities themselves, so pointers stay valid.
    while (!spawned_.empty()) {
        std::vector<std::unique_ptr<Entity>> batch = std::move(spawned_);
        spawned_.clear();
        for (auto& entity : batch) entities_.push_back(std::move(entity));
    }
}

void Scene::removeDestroyed() {
    // An entity's own after()/every() timers stop when it goes away.
    for (const auto& entity : entities_)
        if (entity->isDestroyed())
            for (int id : entity->timers_) cancelTimer(id);
    std::erase_if(entities_, [](const std::unique_ptr<Entity>& e) { return e->isDestroyed(); });
}

// --- Timers --------------------------------------------------------------------------
int Scene::after(float seconds, std::function<void()> action) {
    const int id = nextTimerId_++;
    timers_.push_back(ScheduledAction{id, seconds, seconds, false, false, std::move(action)});
    return id;
}

int Scene::every(float seconds, std::function<void()> action) {
    const int id = nextTimerId_++;
    const float interval = std::max(seconds, 0.001f);
    timers_.push_back(ScheduledAction{id, interval, interval, true, false, std::move(action)});
    return id;
}

void Scene::cancelTimer(int id) {
    for (ScheduledAction& timer : timers_)
        if (timer.id == id) timer.cancelled = true;
}

void Scene::cancelAllTimers() {
    for (ScheduledAction& timer : timers_) timer.cancelled = true;
}

void Scene::updateTimers(float dt) {
    // Index loop: an action may add new timers while we're going through them.
    const std::size_t count = timers_.size();
    for (std::size_t i = 0; i < count; ++i) {
        if (timers_[i].cancelled) continue;
        timers_[i].timeLeft -= dt;
        while (timers_[i].timeLeft <= 0.0f && !timers_[i].cancelled) {
            if (timers_[i].repeat) {
                timers_[i].timeLeft += timers_[i].interval;
            } else {
                timers_[i].cancelled = true; // one-shot: done after this
            }
            std::function<void()> action = timers_[i].action; // copy: the vector may grow
            action();
            if (!timers_[i].repeat) break;
        }
    }
    std::erase_if(timers_, [](const ScheduledAction& t) { return t.cancelled; });
}

// --- Solids --------------------------------------------------------------------------
void Scene::addSolid(const Rect& rect) { solidRects_.push_back(rect); }

void Scene::addSolid(const TileMap& map) {
    if (std::find(solidMaps_.begin(), solidMaps_.end(), &map) == solidMaps_.end()) solidMaps_.push_back(&map);
}

void Scene::clearSolids() {
    solidRects_.clear();
    solidMaps_.clear();
}

// --- The frame -----------------------------------------------------------------------
void Scene::runStart() {
    start();
    mergeSpawned();
}

void Scene::runUpdate(float dt) {
    mergeSpawned(); // entities spawned since the last frame join in now
    update(dt);

    if (!paused) {
        updateTimers(dt);
        mergeSpawned(); // ...and so do ones spawned by update() or a timer, so they update this frame too
        // Entities spawned while the entities update wait in spawned_ until the next frame.
        for (std::size_t i = 0; i < entities_.size(); ++i) {
            Entity& entity = *entities_[i];
            if (!entity.isDestroyed()) entity.update(dt);
        }
        particles.update(dt);
    }

    mergeSpawned();
    removeDestroyed();
    camera.updateCamera(paused ? 0.0f : dt);
}

void Scene::runDraw() {
    toRaylib(background).ClearBackground();

    raylib::Camera2D view = toRaylib(camera);
    view.BeginMode();
    draw();
    // Draw entities from the lowest layer to the highest (stable: same layer keeps spawn order).
    std::vector<Entity*> order;
    order.reserve(entities_.size());
    for (auto& entity : entities_)
        if (!entity->isDestroyed() && entity->visible) order.push_back(entity.get());
    std::stable_sort(order.begin(), order.end(), [](const Entity* a, const Entity* b) { return a->layer < b->layer; });
    for (Entity* entity : order) entity->draw();
    particles.draw();
    view.EndMode();

    drawUI();
}

} // namespace sprout
