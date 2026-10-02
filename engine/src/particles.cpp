#include "sprout/particles.hpp"

#include <algorithm>
#include <cmath>

#include "sprout/graphics.hpp"
#include "sprout/random.hpp"

namespace sprout {

namespace {
// Stops runaway effects from slowing the game down.
constexpr std::size_t kMaxParticles = 5000;
} // namespace

void ParticleSystem::add(const Particle& particle) {
    if (particles_.size() >= kMaxParticles) return;
    particles_.push_back(particle);
}

void ParticleSystem::burst(Vec2 position, Color color, int count, float speed, float lifetime, float size) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.position = position;
        p.velocity = randomDirection() * randomFloat(speed * 0.3f, speed);
        p.color = color;
        p.size = randomFloat(size * 0.6f, size * 1.2f);
        p.maxLife = p.life = randomFloat(lifetime * 0.6f, lifetime);
        add(p);
    }
}

void ParticleSystem::spray(Vec2 position, Color color, float direction, float spread, int count, float speed,
                           float lifetime, float size) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.position = position;
        p.velocity = Vec2::fromAngle(direction + randomFloat(-spread / 2.0f, spread / 2.0f), randomFloat(speed * 0.5f, speed));
        p.color = color;
        p.size = randomFloat(size * 0.6f, size * 1.2f);
        p.maxLife = p.life = randomFloat(lifetime * 0.6f, lifetime);
        add(p);
    }
}

void ParticleSystem::update(float dt) {
    const float slowdown = std::exp(-drag * dt);
    for (Particle& p : particles_) {
        p.velocity.y += gravity * dt;
        p.velocity *= slowdown;
        p.position += p.velocity * dt;
        p.life -= dt;
    }
    std::erase_if(particles_, [](const Particle& p) { return p.life <= 0.0f; });
}

void ParticleSystem::draw() const {
    for (const Particle& p : particles_) {
        const float t = p.maxLife > 0.0f ? std::clamp(p.life / p.maxLife, 0.0f, 1.0f) : 0.0f;
        const float size = shrink ? p.size * t : p.size;
        const Color color = fade ? p.color.withAlpha(t * static_cast<float>(p.color.a) / 255.0f) : p.color;
        if (size >= 2.5f) drawCircle(p.position, size / 2.0f, color);
        else drawRect(Rect::fromCenter(p.position, Vec2{size, size}), color);
    }
}

void ParticleSystem::clear() { particles_.clear(); }

} // namespace sprout
