// Tests for sprout/camera.hpp that work without a window: screen shake.
//
// Camera::updateCamera(dt) is what the scene calls once per frame.
#include <doctest/doctest.h>

#include <algorithm>

#include <sprout/camera.hpp>

using namespace sprout;

namespace {
/// The biggest shake offset seen over a number of frames.
float biggestShake(Camera& camera, int frames, float dt) {
    float biggest = 0.0f;
    for (int i = 0; i < frames; ++i) {
        camera.updateCamera(dt);
        biggest = std::max(biggest, camera.shakeOffset().length());
    }
    return biggest;
}
} // namespace

TEST_CASE("a shake moves the view, fades out and then stops") {
    Camera camera;
    camera.shake(10.0f, 0.5f);
    const float during = biggestShake(camera, 10, 0.01f);
    CHECK(during > 0.0f);
    CHECK(during <= 10.0f);
    biggestShake(camera, 100, 0.01f); // well past the end
    CHECK(camera.shakeOffset() == Vec2{});
}

TEST_CASE("a short strong shake on top of a long weak one doesn't last the long time") {
    Camera camera;
    camera.shake(2.0f, 5.0f);   // gentle rumble for 5 seconds
    camera.shake(20.0f, 0.2f);  // BOOM for 0.2 seconds
    biggestShake(camera, 50, 0.01f); // half a second later the boom is over...
    // ...and only the gentle rumble (at most 2 pixels) is left.
    CHECK(biggestShake(camera, 100, 0.01f) <= 2.0f);
}

TEST_CASE("a weaker shake doesn't cut a stronger one short") {
    Camera camera;
    camera.shake(20.0f, 1.0f);
    camera.shake(1.0f, 0.1f); // much weaker: ignored
    biggestShake(camera, 20, 0.01f); // 0.2 s later
    CHECK(biggestShake(camera, 20, 0.01f) > 1.0f); // the big shake is still going
}

TEST_CASE("the view stays still while the scene is paused") {
    Camera camera;
    camera.shake(10.0f, 1.0f);
    camera.updateCamera(0.01f);
    const Vec2 before = camera.shakeOffset();
    for (int i = 0; i < 30; ++i) {
        camera.updateCamera(0.0f); // what a paused scene does
        CHECK(camera.shakeOffset() == before);
    }
    // ...and the shake carries on after the pause.
    CHECK(biggestShake(camera, 10, 0.01f) > 0.0f);
}
