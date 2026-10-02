#pragma once
// =============================================================================
//  sprout/particles.hpp - sparks, explosions, dust and confetti
//
//  Every scene has one called `particles`, which is updated and drawn for you:
//
//    particles.burst(enemy.position, Color::Orange);         // quick explosion
//    particles.burst(position, Color::Yellow, 50, 300);      // 50 fast sparks
//    particles.gravity = 400;                                 // make them fall
// =============================================================================
#include <vector>

#include "sprout/color.hpp"
#include "sprout/math.hpp"

namespace sprout {

struct Particle {
    Vec2 position;
    Vec2 velocity;
    Color color;
    float size = 4.0f;
    float life = 1.0f;    ///< seconds left
    float maxLife = 1.0f; ///< seconds it lived in total
};

class ParticleSystem {
public:
    /// Pulls particles down (pixels per second, per second). 0 = float in space.
    float gravity = 0.0f;
    /// Slows particles down over time. 0 = never slow down, 3 = stop quickly.
    float drag = 1.0f;
    /// Particles shrink as they die.
    bool shrink = true;
    /// Particles fade out as they die.
    bool fade = true;

    /// Fires `count` particles in every direction from `position`.
    void burst(Vec2 position, Color color, int count = 20, float speed = 150.0f,
               float lifetime = 0.6f, float size = 4.0f);
    /// Fires particles in one direction (degrees), spread out by `spread` degrees.
    /// Nice for rocket flames: particles.spray(pos, Color::Orange, 90, 30);
    void spray(Vec2 position, Color color, float direction, float spread, int count = 5,
               float speed = 150.0f, float lifetime = 0.5f, float size = 3.0f);
    /// Adds one particle exactly how you want it.
    void add(const Particle& particle);

    void update(float dt);
    void draw() const;
    void clear();
    int count() const { return static_cast<int>(particles_.size()); }

private:
    std::vector<Particle> particles_;
};

} // namespace sprout
