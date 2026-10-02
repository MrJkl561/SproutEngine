#pragma once
// =============================================================================
//  sprout/scene.hpp - scenes (screens) of your game
//
//  A scene is one "screen" of your game: the title menu, a level, the
//  game-over screen... Make one by inheriting from Scene:
//
//    struct Level1 : Scene {
//        void start() override { /* runs once when the scene begins */ }
//        void update(float dt) override { /* runs every frame */ }
//        void draw() override { /* draw the world (seen through the camera) */ }
//        void drawUI() override { /* draw score etc. (stuck to the screen) */ }
//    };
//
//  Every frame the engine does this, in order:
//    1. your update(dt)
//    2. update(dt) of every entity
//    3. clear the screen with `background`
//    4. your draw(), then every entity's draw(), then the particles (all through the camera)
//    5. your drawUI() (on top of everything, not affected by the camera)
//
//  Go to another scene with  changeScene<GameOver>();  (see game.hpp)
// =============================================================================
#include <functional>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "sprout/camera.hpp"
#include "sprout/color.hpp"
#include "sprout/entity.hpp"
#include "sprout/math.hpp"
#include "sprout/particles.hpp"

namespace sprout {

class TileMap;

class Scene {
public:
    /// The color the screen is cleared to every frame.
    Color background = Color::Black;
    /// The camera that draw() is seen through.
    Camera camera;
    /// Particle effects - see particles.hpp.
    ParticleSystem particles;
    /// While true: entities, particles and after()/every() timers stop.
    /// Your own update() still runs, so you can un-pause.
    bool paused = false;

    Scene();
    virtual ~Scene();
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // --- Fill these in ----------------------------------------------------------
    /// Runs once when the scene begins.
    virtual void start() {}
    /// Runs every frame, before the entities update.
    virtual void update([[maybe_unused]] float dt) {}
    /// Draws the game world. Entities are drawn after (on top of) this.
    virtual void draw() {}
    /// Draws on top of everything, ignoring the camera. For scores, menus, etc.
    virtual void drawUI() {}

    // --- Entities ---------------------------------------------------------------
    /// Creates an entity and adds it to the scene. Any values you pass go to
    /// the entity's constructor:   spawn<Bullet>(position, Vec2{0, -500});
    template <class T, class... Args>
    T& spawn(Args&&... args);

    /// Every entity of type T (that hasn't been destroyed).
    ///   for (Enemy* enemy : findAll<Enemy>()) { ... }
    template <class T = Entity>
    std::vector<T*> findAll();
    /// The first entity of type T, or nullptr if there are none.
    template <class T = Entity>
    T* findFirst();
    /// The entity of type T closest to a point, or nullptr if there are none.
    template <class T = Entity>
    T* findClosest(Vec2 point);
    /// How many entities of type T there are.
    template <class T = Entity>
    int count();
    /// Destroys every entity of type T (all entities if you leave out <T>).
    template <class T = Entity>
    void destroyAll();

    // --- Timers -----------------------------------------------------------------
    /// Runs some code once, after a delay:
    ///   after(2.0f, [this] { changeScene<GameOver>(); });
    /// Returns an id you can give to cancelTimer().
    int after(float seconds, std::function<void()> action);
    /// Runs some code again and again:
    ///   every(1.5f, [this] { spawn<Enemy>(); });
    int every(float seconds, std::function<void()> action);
    void cancelTimer(int id);
    void cancelAllTimers();

    // --- Solid things for Entity::moveAndSlide() --------------------------------
    /// Makes a rectangle solid: entities using moveAndSlide() can't go through it.
    void addSolid(const Rect& rect);
    /// Makes the solid tiles of a tile map block entities.
    /// (The map must keep existing - make it a member of your scene.)
    void addSolid(const TileMap& map);
    void clearSolids();
    const std::vector<Rect>& solidRects() const { return solidRects_; }
    const std::vector<const TileMap*>& solidMaps() const { return solidMaps_; }

    // --- Used by the engine (you don't need to call these) ----------------------
    void runStart();
    void runUpdate(float dt);
    void runDraw();
    /// Adds an already-made entity. spawn<T>() is easier!
    Entity& addEntity(std::unique_ptr<Entity> entity);

private:
    struct ScheduledAction {
        int id = 0;
        float interval = 0.0f;
        float timeLeft = 0.0f;
        bool repeat = false;
        bool cancelled = false;
        std::function<void()> action;
    };

    template <class Fn>
    void forEachLiving(Fn&& fn);
    void updateTimers(float dt);
    void mergeSpawned();
    void removeDestroyed();

    std::vector<std::unique_ptr<Entity>> entities_;
    std::vector<std::unique_ptr<Entity>> spawned_; // added this frame, joins entities_ soon
    std::vector<ScheduledAction> timers_;
    int nextTimerId_ = 1;
    std::vector<Rect> solidRects_;
    std::vector<const TileMap*> solidMaps_;
};

// =============================================================================
//  Template code (has to live in the header)
// =============================================================================

namespace detail {
/// The entity as a T*, or nullptr if it isn't a T. (dynamic_cast, minus a
/// compiler warning when T is Entity itself.)
template <class T>
T* entityAs(Entity& entity) {
    if constexpr (std::is_same_v<T, Entity>) return &entity;
    else return dynamic_cast<T*>(&entity);
}
} // namespace detail

template <class Fn>
void Scene::forEachLiving(Fn&& fn) {
    for (auto& entity : entities_)
        if (!entity->isDestroyed()) fn(*entity);
    for (auto& entity : spawned_)
        if (!entity->isDestroyed()) fn(*entity);
}

template <class T, class... Args>
T& Scene::spawn(Args&&... args) {
    static_assert(std::is_base_of_v<Entity, T>,
                  "spawn<T>() only works with classes that inherit from Entity, e.g.  struct Enemy : Entity { ... };");
    auto entity = std::make_unique<T>(std::forward<Args>(args)...);
    T& result = *entity;
    addEntity(std::move(entity));
    return result;
}

template <class T>
std::vector<T*> Scene::findAll() {
    std::vector<T*> result;
    forEachLiving([&](Entity& entity) {
        if (T* found = detail::entityAs<T>(entity)) result.push_back(found);
    });
    return result;
}

template <class T>
T* Scene::findFirst() {
    T* result = nullptr;
    forEachLiving([&](Entity& entity) {
        if (!result) result = detail::entityAs<T>(entity);
    });
    return result;
}

template <class T>
T* Scene::findClosest(Vec2 point) {
    T* result = nullptr;
    float best = std::numeric_limits<float>::max();
    forEachLiving([&](Entity& entity) {
        if (T* found = detail::entityAs<T>(entity)) {
            const float distance = (entity.position - point).lengthSquared();
            if (distance < best) {
                best = distance;
                result = found;
            }
        }
    });
    return result;
}

template <class T>
int Scene::count() {
    int result = 0;
    forEachLiving([&](Entity& entity) {
        if (detail::entityAs<T>(entity)) ++result;
    });
    return result;
}

template <class T>
void Scene::destroyAll() {
    forEachLiving([&](Entity& entity) {
        if (detail::entityAs<T>(entity)) entity.destroy();
    });
}

template <class T>
T* Entity::findOverlapping() {
    for (T* other : scene().template findAll<T>())
        if (static_cast<Entity*>(other) != this && overlaps(*other)) return other;
    return nullptr;
}

template <class T>
std::vector<T*> Entity::findAllOverlapping() {
    std::vector<T*> result;
    for (T* other : scene().template findAll<T>())
        if (static_cast<Entity*>(other) != this && overlaps(*other)) result.push_back(other);
    return result;
}

} // namespace sprout
