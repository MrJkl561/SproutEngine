#pragma once
// =============================================================================
//  sprout/random.hpp - random numbers
//
//    randomInt(1, 6)          a dice roll: 1, 2, 3, 4, 5 or 6
//    randomFloat(0, 800)      any number from 0 to 800, like 412.7
//    chance(0.25f)            true 25% of the time
//    randomChoice(names)      a random item from a std::vector
// =============================================================================
#include <cstddef>
#include <stdexcept>
#include <vector>

#include "sprout/color.hpp"
#include "sprout/math.hpp"

namespace sprout {

/// A random whole number from min to max. BOTH ends can be picked.
int randomInt(int min, int max);
/// A random decimal number from min to max.
float randomFloat(float min, float max);
/// True with the given probability: chance(0.5f) is a coin flip.
bool chance(float probability);
/// A random length-1 direction (any angle).
Vec2 randomDirection();
/// A random point inside a rectangle.
Vec2 randomPointIn(const Rect& area);
/// A random bright color.
Color randomColor();
/// Use the same seed to get the same "random" numbers every time you run the game.
void setRandomSeed(unsigned int seed);

/// Picks a random item from a list. The list must not be empty.
template <class T>
T& randomChoice(std::vector<T>& items) {
    if (items.empty()) throw std::out_of_range("randomChoice() was given an empty list");
    return items[static_cast<std::size_t>(randomInt(0, static_cast<int>(items.size()) - 1))];
}
template <class T>
const T& randomChoice(const std::vector<T>& items) {
    if (items.empty()) throw std::out_of_range("randomChoice() was given an empty list");
    return items[static_cast<std::size_t>(randomInt(0, static_cast<int>(items.size()) - 1))];
}

} // namespace sprout
