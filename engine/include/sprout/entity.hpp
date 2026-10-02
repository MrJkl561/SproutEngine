#pragma once
// =============================================================================
//  sprout/entity.hpp - the "things" in your game
//
//  An Entity is anything that lives in a scene: the player, an enemy, a
//  bullet, a coin... Make your own by inheriting from Entity and filling in
//  the parts you need:
//
//    struct Coin : Entity {
//        Coin(Vec2 where) { position = where; size = {16, 16}; }
//
//        void update(float dt) override {
//            // runs every frame
//        }
//        void draw() override {
//            drawCircle(position, 8, Color::Gold);
//        }
//    };
//
//  Then put one in your scene with:   spawn<Coin>(Vec2{100, 200});
//  and remove it with:                coin.destroy();
//
//  IMPORTANT: `position` is the MIDDLE of the entity, and `size` is the size
//  of its hitbox (the invisible box used for collisions).
// =============================================================================
#include <vector>

#include "sprout/math.hpp"
#include "sprout/physics.hpp"

namespace sprout {

class Scene;

class Entity {
public:
    Vec2 position;           ///< the middle of the entity, in world coordinates
    Vec2 velocity;           ///< speed in pixels per second (used by moveAndSlide)
    Vec2 size{32.0f, 32.0f}; ///< hitbox width and height
    int layer = 0;           ///< higher layers are drawn on top of lower ones
    bool visible = true;     ///< invisible entities still update, they just aren't drawn

    Entity() = default;
    virtual ~Entity() = default;
    Entity(const Entity&) = delete;            // entities live in a scene and
    Entity& operator=(const Entity&) = delete; // shouldn't be copied

    // --- Fill these in ----------------------------------------------------------
    /// Runs once, right after the entity is spawned. scene() works here.
    virtual void start() {}
    /// Runs every frame. dt is the time since the last frame, in seconds.
    virtual void update([[maybe_unused]] float dt) {}
    /// Draw the entity here.
    virtual void draw() {}

    // --- Collisions -------------------------------------------------------------
    /// The hitbox: a rectangle of `size`, centered on `position`.
    Rect bounds() const;
    /// True if the hitboxes overlap.
    bool overlaps(const Entity& other) const;
    bool overlaps(const Rect& area) const;
    /// The first entity of type T touching this one, or nullptr if none.
    ///   if (Coin* coin = findOverlapping<Coin>()) { coin->destroy(); score++; }
    template <class T>
    T* findOverlapping();
    /// Every entity of type T touching this one.
    template <class T>
    std::vector<T*> findAllOverlapping();

    /// Moves by velocity * dt, stopping at walls (the scene's solids - see
    /// Scene::addSolid). Hitting a wall/floor/ceiling sets that part of the
    /// velocity to 0, so the entity slides along it.
    MoveResult moveAndSlide(float dt);
    /// After moveAndSlide(): standing on something?
    bool isOnFloor() const { return lastMove_.hitBottom; }
    /// After moveAndSlide(): touching a wall on the left or right?
    bool isOnWall() const { return lastMove_.hitWall(); }
    /// After moveAndSlide(): bumped a ceiling?
    bool isOnCeiling() const { return lastMove_.hitTop; }

    // --- Life and death ---------------------------------------------------------
    /// Removes the entity from the scene at the end of this frame.
    void destroy() { destroyed_ = true; }
    bool isDestroyed() const { return destroyed_; }

    /// The scene this entity lives in. (Not available inside the constructor -
    /// use start() for things that need the scene.)
    Scene& scene() const;

private:
    friend class Scene;
    Scene* scene_ = nullptr;
    bool destroyed_ = false;
    MoveResult lastMove_;
};

} // namespace sprout

// Entity's template functions need the full Scene class.
#include "sprout/scene.hpp"
