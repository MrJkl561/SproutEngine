# 14. Math and random numbers

Don't worry, this isn't a math lesson! Games use a handful of simple math
ideas over and over, and Sprout has a ready-made helper for each of them. This
page explains them with pictures and shows recipes you can copy.

---

## `Vec2`: a point, a size, a speed or a direction

A `Vec2` is just two numbers, `x` and `y`. The same type is used for:

- a **point**: where something is (`position`)
- a **size**: how wide and tall something is (`size`)
- a **velocity**: how fast something moves, sideways and up/down
- a **direction**: which way something is pointing

You can picture a `Vec2` as an **arrow**: go `x` pixels to the right and `y`
pixels down.

```
       x = 3
  ●──────────►
              │
              │ y = 4        Vec2{3, 4}: an arrow 5 pixels long
              ▼
```

### Making one

```cpp
Vec2 a{3, 4};
Vec2 b = {1.5f, 2};
position = {400, 300};   // works anywhere a Vec2 is expected
Vec2 zero;               // {0, 0}
```

### Math with `Vec2`

| Code | Result | What it's good for |
|---|---|---|
| `a + b` | `{a.x + b.x, a.y + b.y}` | moving a point by an arrow |
| `a - b` | `{a.x - b.x, a.y - b.y}` | the arrow from `b` to `a` |
| `a * 2`, `2 * a` | twice as long | speeding up |
| `a / 2` | half as long | slowing down |
| `-a` | pointing the other way | bouncing back |
| `a * b` | `{a.x * b.x, a.y * b.y}` | stretching differently in x and y |
| `a += b`, `a -= b`, `a *= 2`, `a /= 2` | change `a` itself | `position += velocity * dt;` |
| `a == b` | are they exactly the same? | |

### Useful functions

| Code | What you get |
|---|---|
| `v.length()` | How long the arrow is. `Vec2{3, 4}.length()` is `5`. |
| `v.lengthSquared()` | The length times itself. Faster; fine for comparing distances. |
| `v.normalized()` | Same direction, but exactly 1 long. (`{0, 0}` stays `{0, 0}`.) |
| `a.distanceTo(b)` | The distance between two points |
| `a.directionTo(b)` | A 1-long arrow pointing from `a` towards `b` |
| `v.angle()` | Which way the arrow points, in degrees |
| `a.angleTo(b)` | The angle from point `a` to point `b`, in degrees |
| `v.rotated(degrees)` | The arrow turned (positive = clockwise on screen) |
| `a.dot(b)` | "Dot product": positive if they point the same-ish way, negative if opposite-ish |
| `Vec2::fromAngle(degrees, length)` | An arrow pointing at an angle. `Vec2::fromAngle(90, 5)` is `{0, 5}`. |

**Why "1 long" (normalized) arrows are useful:** a direction of length 1 times a
speed gives a velocity with exactly that speed. That's how `getVector` (see
[Input](06-input.md)) stops diagonal movement from being faster.

### Angles

Angles in Sprout are always in **degrees**. **0 points right** and **90 points
down** (because y goes down on the screen), so positive angles turn
**clockwise**:

```
            270 (or -90): up
                  ▲
                  │
 180: left ◄──────┼──────► 0: right
                  │
                  ▼
              90: down
```

If you ever need radians (another way of measuring angles, used by the C++
`std::sin` and `std::cos` functions), use `toRadians(degrees)` and
`toDegrees(radians)`. `Pi` is the number π (3.14159...).

---

## `Rect`: a rectangle

A `Rect` has `x`, `y`, `width` and `height`. **`x` and `y` are its top-left
corner.**

```cpp
Rect wall{100, 300, 200, 20};                     // x, y, width, height
Rect box{Vec2{100, 300}, Vec2{200, 20}};          // top-left corner, size
Rect around = Rect::fromCenter({400, 225}, {100, 50}); // a rectangle around a middle point
```

| Code | What you get |
|---|---|
| `r.left()`, `r.right()`, `r.top()`, `r.bottom()` | Its edges |
| `r.position()` | Its top-left corner, as a `Vec2` |
| `r.size()` | `{width, height}` |
| `r.center()` | Its middle |
| `r.setCenter(point)` | Moves it so its middle is at `point` |
| `r.overlaps(other)` | Do two rectangles overlap? (Only touching edges doesn't count.) |
| `r.contains(point)` | Is a point inside? (Great for "was this clicked?") |
| `r.grown(10)` | A copy that's 10 pixels bigger on every side (`grown(-10)` shrinks it) |

An entity's `bounds()` gives you its hitbox as a `Rect`.

---

## Handy helpers

### `clamp`: keep a value between limits

```cpp
clamp(150, 0, 100)    // 100  (too big, so you get the biggest allowed)
clamp(-5, 0, 100)     // 0    (too small, so you get the smallest allowed)
clamp(42, 0, 100)     // 42   (fine as it is)

health = clamp(health + 25, 0, 100);                  // healing, never above 100
position = clamp(position, Rect{0, 0, 800, 450});     // keep a point inside a rectangle
```

`clamp` gives back a `float`. To store the result in an `int`, write
`hp = int(clamp(hp + 5, 0, 10));`.

### `lerp`: blend between two values

"Lerp" is short for "linear interpolation", which just means "go part of the
way". `t = 0` gives the start, `t = 1` gives the end, `t = 0.5` is halfway:

```cpp
lerp(0.0f, 10.0f, 0.25f)                   // 2.5
lerp(Vec2{0, 0}, Vec2{10, 20}, 0.5f)       // {5, 10}
Color::mix(Color::Red, Color::Blue, 0.5f)  // purple (the same idea for colors)
```

Great for health bars that fill smoothly, fading, and animations from A to B
(use a `Timer`'s `progress()` as `t`!).

### `moveToward`: get closer, but not too fast

```cpp
moveToward(0, 300, 50)                       // 50: one step of at most 50 towards 300
position = moveToward(position, target, 200 * dt);  // walk towards a point at 200 pixels/second
speed = moveToward(speed, 300, 600 * dt);           // speed up smoothly to 300
speed = moveToward(speed, 0, 900 * dt);             // slow down smoothly to 0 (friction)
```

`moveToward` never overshoots: once it reaches the target, it stays there. It
works with numbers and with points.

### `wrap`: go off one side, come back on the other

```cpp
wrap(810, 0, 800)   // 10
wrap(-10, 0, 800)   // 790

// Asteroids / Pac-Man style screen wrapping:
position.x = wrap(position.x, 0, 800);
position.y = wrap(position.y, 0, 450);
```

---

## Random numbers

Randomness keeps games surprising: where enemies appear, how much damage a hit
does, which way sparks fly.

| Code | What you get |
|---|---|
| `randomInt(1, 6)` | A whole number from 1 to 6, **both ends included** (a dice roll) |
| `randomFloat(0, 800)` | A decimal number from 0 to 800, like 412.7 |
| `chance(0.25f)` | `true` 25% of the time (`chance(0.5f)` is a coin flip) |
| `randomDirection()` | An arrow 1 long, pointing any which way |
| `randomPointIn(rect)` | A random point inside a rectangle |
| `randomColor()` | A random bright color |
| `randomChoice(list)` | A random item from a `std::vector` (the list must not be empty) |
| `setRandomSeed(42)` | Makes the "random" numbers the same every time you run the game |

```cpp
// An enemy at a random spot along the top of the screen
spawn<Enemy>(Vec2{randomFloat(20, 780), -20});

// 10% of coins are golden
bool golden = chance(0.1f);

// A random name
std::vector<std::string> names = {"Ana", "Bo", "Cy"};
std::string name = randomChoice(names);

// Damage from 8 to 12
int damage = randomInt(8, 12);
```

**Seeds:** computers can't really make random numbers; they calculate numbers
that *look* random, starting from a number called the **seed**. Sprout picks a
different seed every time the game starts. If you set the seed yourself with
`setRandomSeed(42)`, you get the same "random" numbers every run. That's useful
for testing, or for a "level of the day" that's the same for everybody.

---

## Recipes

**Chase something:**

```cpp
position = moveToward(position, player->position, speed * dt);
```

**Shoot towards the mouse:**

```cpp
Vec2 aim = position.directionTo(mouseWorldPosition());
scene().spawn<Bullet>(position, aim * 600);
```

**Point a picture at the mouse:**

```cpp
sprite.rotation = position.angleTo(mouseWorldPosition());
```

**Is something close?**

```cpp
if (position.distanceTo(player->position) < 100) { /* the player is near! */ }
```

**Circle around a point** (90 degrees per second, 100 pixels away):

```cpp
position = Vec2{400, 225} + Vec2::fromAngle(timeSinceStart() * 90, 100);
```

**Bounce off the edges of the screen:**

```cpp
position += velocity * dt;
if (position.x < 0 || position.x > 800) velocity.x = -velocity.x;
if (position.y < 0 || position.y > 450) velocity.y = -velocity.y;
```

(`||` means "or".)

**Spawn on the left or right edge:**

```cpp
float side = randomInt(0, 1) == 0 ? -20.0f : 820.0f;
spawn<Enemy>(Vec2{side, randomFloat(0, 450)});
```

**Pick something with different chances** (60% common, 30% rare, 10% epic):

```cpp
int roll = randomInt(1, 100);
if (roll <= 60) { /* common */ }
else if (roll <= 90) { /* rare */ }
else { /* epic! */ }
```

**Next:** keep the [Cheat sheet](cheat-sheet.md) open while you make your own
games!
