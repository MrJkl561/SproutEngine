#pragma once
// =============================================================================
//  sprout/math.hpp - points, rectangles and handy math helpers
//
//  Coordinates work like this (the same as almost every 2D engine):
//
//      (0,0) ------> x gets bigger to the RIGHT
//        |
//        |
//        v  y gets bigger going DOWN
//
//  Angles are always in DEGREES. 0 = pointing right, 90 = pointing down.
// =============================================================================
#include <algorithm>
#include <cmath>
#include <concepts>
#include <type_traits>

namespace sprout {

/// The number pi (3.14159...).
inline constexpr float Pi = 3.14159265358979323846f;

namespace detail {
template <class T>
concept Number = std::is_arithmetic_v<T>;
}

// -----------------------------------------------------------------------------
//  Vec2 - a 2D point or direction: { x, y }
// -----------------------------------------------------------------------------
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2() = default;

    /// Vec2{100, 200}. Works with ints, floats or doubles.
    template <detail::Number X, detail::Number Y>
    constexpr Vec2(X xValue, Y yValue) : x(static_cast<float>(xValue)), y(static_cast<float>(yValue)) {}

    // Math with other vectors: a + b, a - b, a * 2, a / 2, -a ...
    constexpr Vec2 operator+(Vec2 other) const { return {x + other.x, y + other.y}; }
    constexpr Vec2 operator-(Vec2 other) const { return {x - other.x, y - other.y}; }
    constexpr Vec2 operator*(Vec2 other) const { return {x * other.x, y * other.y}; }
    constexpr Vec2 operator/(Vec2 other) const { return {x / other.x, y / other.y}; }
    constexpr Vec2 operator*(float amount) const { return {x * amount, y * amount}; }
    constexpr Vec2 operator/(float amount) const { return {x / amount, y / amount}; }
    constexpr Vec2 operator-() const { return {-x, -y}; }

    constexpr Vec2& operator+=(Vec2 other) { x += other.x; y += other.y; return *this; }
    constexpr Vec2& operator-=(Vec2 other) { x -= other.x; y -= other.y; return *this; }
    constexpr Vec2& operator*=(float amount) { x *= amount; y *= amount; return *this; }
    constexpr Vec2& operator/=(float amount) { x /= amount; y /= amount; return *this; }

    constexpr bool operator==(const Vec2&) const = default;

    /// How long the vector is (distance from {0,0}).
    float length() const;
    /// length() squared. Faster, good for comparing distances.
    constexpr float lengthSquared() const { return x * x + y * y; }
    /// Same direction, but with a length of exactly 1. {0,0} stays {0,0}.
    Vec2 normalized() const;
    /// Distance between this point and another point.
    float distanceTo(Vec2 other) const;
    /// A length-1 vector pointing from this point towards another point.
    Vec2 directionTo(Vec2 other) const;
    /// The angle this vector points at, in degrees (0 = right, 90 = down).
    float angle() const;
    /// The angle from this point to another point, in degrees.
    float angleTo(Vec2 other) const;
    /// This vector turned by some degrees (positive = clockwise on screen).
    Vec2 rotated(float degrees) const;
    /// Dot product. Positive = same-ish direction, negative = opposite-ish.
    constexpr float dot(Vec2 other) const { return x * other.x + y * other.y; }

    /// Makes a vector from an angle in degrees: Vec2::fromAngle(90, 5) is {0, 5}.
    static Vec2 fromAngle(float degrees, float length = 1.0f);
};

/// 2 * vec works as well as vec * 2.
constexpr Vec2 operator*(float amount, Vec2 v) { return v * amount; }

// -----------------------------------------------------------------------------
//  Rect - a rectangle. x and y are the TOP-LEFT corner.
// -----------------------------------------------------------------------------
struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    constexpr Rect() = default;

    /// Rect{x, y, width, height} - x and y are the top-left corner.
    template <detail::Number X, detail::Number Y, detail::Number W, detail::Number H>
    constexpr Rect(X xValue, Y yValue, W w, H h)
        : x(static_cast<float>(xValue)), y(static_cast<float>(yValue)),
          width(static_cast<float>(w)), height(static_cast<float>(h)) {}

    /// Rect{topLeft, size}
    constexpr Rect(Vec2 topLeft, Vec2 size) : x(topLeft.x), y(topLeft.y), width(size.x), height(size.y) {}

    /// A rectangle whose MIDDLE is at `center`.
    static constexpr Rect fromCenter(Vec2 center, Vec2 size) {
        return Rect{center.x - size.x / 2.0f, center.y - size.y / 2.0f, size.x, size.y};
    }

    constexpr float left() const { return x; }
    constexpr float right() const { return x + width; }
    constexpr float top() const { return y; }
    constexpr float bottom() const { return y + height; }

    constexpr Vec2 position() const { return {x, y}; }
    constexpr Vec2 size() const { return {width, height}; }
    constexpr Vec2 center() const { return {x + width / 2.0f, y + height / 2.0f}; }

    /// Moves the rectangle so its middle is at `newCenter`.
    constexpr void setCenter(Vec2 newCenter) {
        x = newCenter.x - width / 2.0f;
        y = newCenter.y - height / 2.0f;
    }

    /// True if the two rectangles overlap. Rectangles that only touch edges don't count.
    constexpr bool overlaps(const Rect& other) const {
        return x < other.x + other.width && other.x < x + width &&
               y < other.y + other.height && other.y < y + height;
    }

    /// True if the point is inside the rectangle.
    constexpr bool contains(Vec2 point) const {
        return point.x >= x && point.x < x + width && point.y >= y && point.y < y + height;
    }

    /// A copy that is bigger on every side by `amount` (use a negative number to shrink).
    constexpr Rect grown(float amount) const {
        return Rect{x - amount, y - amount, width + amount * 2.0f, height + amount * 2.0f};
    }

    constexpr bool operator==(const Rect&) const = default;
};

// -----------------------------------------------------------------------------
//  Helper functions
// -----------------------------------------------------------------------------

/// Keeps a value between min and max:  clamp(150, 0, 100) == 100
constexpr float clamp(float value, float min, float max) {
    return value < min ? min : (value > max ? max : value);
}

/// Keeps a point inside a rectangle.
constexpr Vec2 clamp(Vec2 point, const Rect& area) {
    return {clamp(point.x, area.left(), area.right()), clamp(point.y, area.top(), area.bottom())};
}

/// Blends from a to b. t = 0 gives a, t = 1 gives b, t = 0.5 is halfway.
using std::lerp;
/// Blends between two points. t = 0 gives a, t = 1 gives b.
constexpr Vec2 lerp(Vec2 a, Vec2 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }

/// Moves `current` towards `target`, but never by more than `maxStep`.
/// Great for smooth speeding up / slowing down:  speed = moveToward(speed, 300, 600 * dt);
float moveToward(float current, float target, float maxStep);
/// Moves a point towards a target point, but never further than `maxStep`.
Vec2 moveToward(Vec2 current, Vec2 target, float maxStep);

/// Wraps a value around, like Pac-Man going off one side of the screen:
/// wrap(810, 0, 800) == 10
float wrap(float value, float min, float max);

/// Degrees -> radians and radians -> degrees.
constexpr float toRadians(float degrees) { return degrees * (Pi / 180.0f); }
constexpr float toDegrees(float radians) { return radians * (180.0f / Pi); }

} // namespace sprout
