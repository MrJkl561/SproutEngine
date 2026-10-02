#pragma once
// =============================================================================
//  sprout/camera.hpp - the 2D camera
//
//  Every scene has a camera called `camera`. Whatever you draw in draw() is
//  seen through it. (drawUI() is NOT - it's stuck to the screen, perfect for
//  scores and health bars.)
//
//  By default the camera looks at the middle of the window, so world
//  coordinates and screen coordinates are the same. Then you can:
//
//    camera.follow(player.position);          // stick to the player
//    camera.follow(player.position, 5.0f);    // follow smoothly
//    camera.zoom = 2.0f;                      // zoom in
//    camera.shake(10, 0.3f);                  // BOOM! shake for 0.3 seconds
//    camera.setLimits(level.bounds());        // never show outside the level
// =============================================================================
#include <optional>

#include "sprout/math.hpp"

namespace sprout {

class Camera {
public:
    /// The world point shown in the MIDDLE of the screen.
    Vec2 position;
    /// 1 = normal, 2 = everything twice as big, 0.5 = see twice as much.
    float zoom = 1.0f;
    /// Tilts the view, in degrees.
    float rotation = 0.0f;

    /// Moves the camera to look at `target`.
    /// smoothness 0 = jump there instantly. Try 3 - 10 for a smooth follow
    /// (bigger numbers catch up faster).
    void follow(Vec2 target, float smoothness = 0.0f);
    /// Shakes the screen. strength is in pixels.
    void shake(float strength = 8.0f, float duration = 0.25f);
    /// Stops the camera from ever showing anything outside `area`.
    void setLimits(const Rect& area);
    void clearLimits();

    /// Converts a point on the screen to a point in the world (and back).
    Vec2 screenToWorld(Vec2 screenPoint) const;
    Vec2 worldToScreen(Vec2 worldPoint) const;
    /// The part of the world that is visible right now.
    Rect visibleArea() const;

    /// Puts the camera back to the start: looking at the middle of the window, no zoom.
    void reset();

    // --- used by the engine --------------------------------------------------
    void updateCamera(float dt);
    Vec2 shakeOffset() const { return shakeOffset_; }

private:
    void applyLimits();

    std::optional<Rect> limits_;
    float shakeStrength_ = 0.0f;
    float shakeTimeLeft_ = 0.0f;
    float shakeDuration_ = 0.0f;
    Vec2 shakeOffset_;
};

} // namespace sprout
