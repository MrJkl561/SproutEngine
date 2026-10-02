#pragma once
// =============================================================================
//  sprout/timer.hpp - waiting for things
//
//  Timer - "has enough time passed yet?"  (no need to update it yourself)
//
//      Timer spawnTimer{2.0f};               // 2 seconds, starts right away
//      if (spawnTimer.isDone()) {
//          spawn<Enemy>();
//          spawnTimer.restart();             // wait another 2 seconds
//      }
//
//  Cooldown - "am I allowed to do this again yet?"  (ready straight away)
//
//      Cooldown gun{0.25f};                  // at most 4 shots per second
//      if (isActionDown("shoot") && gun.use()) {
//          spawn<Bullet>(position);
//      }
//
//  Scenes also have after(seconds, ...) and every(seconds, ...) - see scene.hpp.
//
//  Both use game time, so they stop counting while the scene is paused.
// =============================================================================

namespace sprout {

class Timer {
public:
    /// A timer that starts counting now and is done after `seconds`.
    explicit Timer(float seconds = 1.0f);

    /// True once the time is up.
    bool isDone() const;
    /// Starts counting again from zero.
    void restart();
    /// Starts counting again, with a new length.
    void restart(float seconds);
    /// Seconds left until done (0 when done).
    float timeLeft() const;
    /// Seconds since the timer (re)started.
    float elapsed() const;
    /// How far along it is: 0.0 at the start, 1.0 when done. Great for progress bars.
    float progress() const;
    float duration() const { return duration_; }

private:
    float duration_;
    double startTime_;
};

class Cooldown {
public:
    /// Something that can be used once every `seconds`. Starts ready.
    explicit Cooldown(float seconds = 1.0f);

    /// If ready: starts the cooldown and returns true. Otherwise returns false.
    bool use();
    /// True if use() would succeed right now.
    bool isReady() const;
    /// Makes it ready immediately.
    void reset();
    /// Seconds until it's ready again.
    float timeLeft() const;
    float duration() const { return duration_; }
    void setDuration(float seconds) { duration_ = seconds; }

private:
    float duration_;
    double lastUse_;
    bool used_ = false;
};

} // namespace sprout
