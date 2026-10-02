#include "sprout/random.hpp"

#include <random>
#include <utility>

namespace sprout {

namespace {
std::mt19937& generator() {
    static std::mt19937 engine{std::random_device{}()};
    return engine;
}
} // namespace

int randomInt(int min, int max) {
    if (min > max) std::swap(min, max);
    return std::uniform_int_distribution<int>{min, max}(generator());
}

float randomFloat(float min, float max) {
    if (min > max) std::swap(min, max);
    if (min == max) return min;
    return std::uniform_real_distribution<float>{min, max}(generator());
}

bool chance(float probability) {
    if (probability <= 0.0f) return false;
    if (probability >= 1.0f) return true;
    return randomFloat(0.0f, 1.0f) < probability;
}

Vec2 randomDirection() { return Vec2::fromAngle(randomFloat(0.0f, 360.0f)); }

Vec2 randomPointIn(const Rect& area) {
    return {randomFloat(area.left(), area.right()), randomFloat(area.top(), area.bottom())};
}

Color randomColor() { return Color::fromHSV(randomFloat(0.0f, 360.0f), randomFloat(0.6f, 1.0f), 1.0f); }

void setRandomSeed(unsigned int seed) { generator().seed(seed); }

} // namespace sprout
