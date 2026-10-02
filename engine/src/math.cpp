#include "sprout/math.hpp"

#include <cmath>

namespace sprout {

float Vec2::length() const { return std::sqrt(x * x + y * y); }

Vec2 Vec2::normalized() const {
    const float len = length();
    if (len == 0.0f) return {};
    return {x / len, y / len};
}

float Vec2::distanceTo(Vec2 other) const { return (other - *this).length(); }

Vec2 Vec2::directionTo(Vec2 other) const { return (other - *this).normalized(); }

float Vec2::angle() const { return toDegrees(std::atan2(y, x)); }

float Vec2::angleTo(Vec2 other) const { return (other - *this).angle(); }

Vec2 Vec2::rotated(float degrees) const {
    const float r = toRadians(degrees);
    const float c = std::cos(r);
    const float s = std::sin(r);
    return {x * c - y * s, x * s + y * c};
}

Vec2 Vec2::fromAngle(float degrees, float length) {
    const float r = toRadians(degrees);
    return {std::cos(r) * length, std::sin(r) * length};
}

float moveToward(float current, float target, float maxStep) {
    if (std::abs(target - current) <= maxStep) return target;
    return current + (target > current ? maxStep : -maxStep);
}

Vec2 moveToward(Vec2 current, Vec2 target, float maxStep) {
    const Vec2 difference = target - current;
    const float distance = difference.length();
    if (distance <= maxStep || distance == 0.0f) return target;
    return current + difference / distance * maxStep;
}

float wrap(float value, float min, float max) {
    const float range = max - min;
    if (range <= 0.0f) return min;
    float result = std::fmod(value - min, range);
    if (result < 0.0f) result += range;
    if (result >= range) result = 0.0f; // rounding can land exactly on max: that's min again
    return result + min;
}

} // namespace sprout
