#include "sprout/camera.hpp"

#include <algorithm>
#include <cmath>

#include "internal.hpp"
#include "sprout/game.hpp"
#include "sprout/random.hpp"

namespace sprout {

raylib::Camera2D toRaylib(const Camera& camera) {
    return raylib::Camera2D{toRaylib(screenCenter() + camera.shakeOffset()), toRaylib(camera.position),
                            camera.rotation, camera.zoom <= 0.0f ? 0.0001f : camera.zoom};
}

void Camera::follow(Vec2 target, float smoothness) {
    if (smoothness <= 0.0f) {
        position = target;
    } else {
        // Exponential smoothing: looks the same no matter the frame rate.
        const float t = 1.0f - std::exp(-smoothness * deltaTime());
        position = lerp(position, target, t);
    }
    applyLimits();
}

void Camera::shake(float strength, float duration) {
    // A new shake only replaces the current one if it is at least as strong as the
    // current one is RIGHT NOW (shakes fade out). Otherwise a short strong shake on top
    // of a long weak one would keep the strong strength for the long time.
    const float currentStrength =
        shakeTimeLeft_ > 0.0f ? shakeStrength_ * std::clamp(shakeTimeLeft_ / shakeDuration_, 0.0f, 1.0f) : 0.0f;
    if (strength < currentStrength) return;
    shakeStrength_ = strength;
    shakeDuration_ = std::max(duration, 0.0001f);
    shakeTimeLeft_ = duration;
}

void Camera::setLimits(const Rect& area) {
    limits_ = area;
    applyLimits();
}

void Camera::clearLimits() { limits_.reset(); }

void Camera::applyLimits() {
    if (!limits_) return;
    const float z = zoom <= 0.0f ? 0.0001f : zoom;
    const Vec2 halfView = screenSize() / (2.0f * z);
    const Rect& area = *limits_;
    // If the level is smaller than the screen, just center on it.
    if (area.width <= halfView.x * 2.0f) position.x = area.center().x;
    else position.x = clamp(position.x, area.left() + halfView.x, area.right() - halfView.x);
    if (area.height <= halfView.y * 2.0f) position.y = area.center().y;
    else position.y = clamp(position.y, area.top() + halfView.y, area.bottom() - halfView.y);
}

void Camera::updateCamera(float dt) {
    applyLimits();
    if (dt <= 0.0f) return; // paused: keep the view still (and the shake waits too)
    if (shakeTimeLeft_ > 0.0f) {
        shakeTimeLeft_ -= dt;
        const float fade = std::clamp(shakeTimeLeft_ / shakeDuration_, 0.0f, 1.0f);
        shakeOffset_ = randomDirection() * randomFloat(0.0f, shakeStrength_ * fade);
        if (shakeTimeLeft_ <= 0.0f) {
            shakeStrength_ = 0.0f;
            shakeOffset_ = {};
        }
    } else {
        shakeOffset_ = {};
    }
}

Vec2 Camera::screenToWorld(Vec2 screenPoint) const {
    if (!detail::windowIsOpen()) return screenPoint;
    return fromRaylib(toRaylib(*this).GetScreenToWorld(toRaylib(screenPoint)));
}

Vec2 Camera::worldToScreen(Vec2 worldPoint) const {
    if (!detail::windowIsOpen()) return worldPoint;
    return fromRaylib(toRaylib(*this).GetWorldToScreen(toRaylib(worldPoint)));
}

Rect Camera::visibleArea() const {
    const Vec2 size = screenSize();
    const Vec2 corners[4] = {screenToWorld({0, 0}), screenToWorld({size.x, 0}), screenToWorld({0, size.y}),
                             screenToWorld(size)};
    float left = corners[0].x, right = corners[0].x, top = corners[0].y, bottom = corners[0].y;
    for (const Vec2& c : corners) {
        left = std::min(left, c.x);
        right = std::max(right, c.x);
        top = std::min(top, c.y);
        bottom = std::max(bottom, c.y);
    }
    return Rect{left, top, right - left, bottom - top};
}

void Camera::reset() {
    position = screenCenter();
    zoom = 1.0f;
    rotation = 0.0f;
    limits_.reset();
    shakeStrength_ = shakeTimeLeft_ = 0.0f;
    shakeOffset_ = {};
}

} // namespace sprout
