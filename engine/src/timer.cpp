#include "sprout/timer.hpp"

#include <algorithm>

#include "sprout/game.hpp"

namespace sprout {

// --- Timer -------------------------------------------------------------------------
Timer::Timer(float seconds) : duration_(std::max(0.0f, seconds)), startTime_(detail::gameClock()) {}

bool Timer::isDone() const { return elapsed() >= duration_; }

void Timer::restart() { startTime_ = detail::gameClock(); }

void Timer::restart(float seconds) {
    duration_ = std::max(0.0f, seconds);
    restart();
}

float Timer::elapsed() const { return static_cast<float>(detail::gameClock() - startTime_); }

float Timer::timeLeft() const { return std::max(0.0f, duration_ - elapsed()); }

float Timer::progress() const {
    if (duration_ <= 0.0f) return 1.0f;
    return std::clamp(elapsed() / duration_, 0.0f, 1.0f);
}

// --- Cooldown ----------------------------------------------------------------------
Cooldown::Cooldown(float seconds) : duration_(std::max(0.0f, seconds)), lastUse_(0.0) {}

bool Cooldown::isReady() const { return !used_ || detail::gameClock() - lastUse_ >= duration_; }

bool Cooldown::use() {
    if (!isReady()) return false;
    used_ = true;
    lastUse_ = detail::gameClock();
    return true;
}

void Cooldown::reset() { used_ = false; }

float Cooldown::timeLeft() const {
    if (!used_) return 0.0f;
    return std::max(0.0f, duration_ - static_cast<float>(detail::gameClock() - lastUse_));
}

} // namespace sprout
