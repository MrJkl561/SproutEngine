# 10. Camera and effects

This page covers the **camera**, which lets your world be bigger than the
window, and **particles**, the sparks, smoke and confetti that make a game feel
"juicy". Screen shake lives here too.

---

## The camera

Think of the camera as a movie camera filming your game world. The window shows
whatever the camera is looking at.

Every scene has one, called `camera`. Everything you draw in `draw()` (and all
entities and particles) is seen **through** the camera. `drawUI()` is **not**:
it's stuck to the screen, like a sticker on the lens. That's why scores go in
`drawUI()`.

When a scene starts, the camera looks at the middle of the window with no zoom,
so world coordinates and screen coordinates are exactly the same. You only need
to think about the camera when you start moving it. (If you let the player
resize the window with `setResizable(true)`, a camera you haven't moved keeps
looking at the new middle, so the two stay the same.)

| Member | What it is | Starts as |
|---|---|---|
| `camera.position` | The world point shown in the **middle** of the screen | the middle of the window |
| `camera.zoom` | 1 = normal, 2 = everything twice as big, 0.5 = see twice as much | `1` |
| `camera.rotation` | Tilts the view, in degrees | `0` |

### Following the player

```cpp
void update(float dt) override {
    if (Player* player = findFirst<Player>()) {
        camera.follow(player->position);      // stick to the player exactly
    }
}
```

Call `follow` every frame. For a softer, more cinematic feel, give it a
**smoothness** number:

```cpp
camera.follow(player->position, 5); // glides after the player
```

`0` (the default) jumps straight there. Numbers from about 3 to 10 make the
camera glide; bigger numbers catch up faster. Smooth following works the same at
any frame rate.

You can also put the `follow` call in the player's own `update()`:
`scene().camera.follow(position, 5);`.

### Staying inside the level

When the player walks to the edge of the level, a following camera would show
the empty space outside it. `setLimits` stops that:

```cpp
camera.setLimits(map.bounds()); // never show anything outside the tile map
camera.setLimits(Rect{0, 0, 2000, 1200}); // or any rectangle
```

If the level is smaller than the screen, the camera simply stays centered on it.
`camera.clearLimits()` removes the limits again.

### Zoom

```cpp
camera.zoom = 2.0f;   // zoom in: everything twice as big
camera.zoom = 0.5f;   // zoom out: see twice as much

// Zoom with the mouse wheel, but not too far in or out:
camera.zoom = clamp(camera.zoom + mouseWheel() * 0.1f, 0.5f, 3.0f);
```

Zooming in is great for pixel-art games: draw everything small, then set
`camera.zoom = 3;` to make it big and chunky.

### Screen shake

Nothing says "BOOM!" like a shaking screen:

```cpp
camera.shake();            // a small shake (8 pixels, a quarter of a second)
camera.shake(10, 0.3f);    // 10 pixels for 0.3 seconds
camera.shake(25, 1.0f);    // an earthquake
```

The shake fades out by itself. Only the world shakes; your `drawUI()` stays
still. From inside an entity, use `scene().camera.shake(...)`.

If a shake is already going on, a new one only takes over when it's at least as
strong as the current (fading) shake, so a tiny bump can't cut a big explosion
short. While the scene is [paused](04-scenes.md#pausing), the shake waits and
carries on afterwards.

> **Don't overdo it.** Small shakes (4 to 10 pixels) for hits and explosions feel
> great. Big shakes on every action make people feel seasick.

### Screen and world coordinates

Once the camera moves, a point on the screen and a point in the world are no
longer the same. These convert between them:

| Code | What it gives you |
|---|---|
| `mouseWorldPosition()` | Where the mouse is **in the world**. Use it to click on things or aim at the mouse. |
| `camera.screenToWorld(point)` | Converts any screen point to a world point |
| `camera.worldToScreen(point)` | Converts a world point to a screen point (for example to draw a name tag in `drawUI()` above an entity) |
| `camera.visibleArea()` | The part of the world that's visible right now, as a `Rect` |
| `camera.reset()` | Back to the start: looking at the middle of the window, no zoom, no rotation, no limits |

```cpp
// A name tag that follows an enemy, drawn in drawUI() so it never gets zoomed:
void drawUI() override {
    for (Enemy* enemy : findAll<Enemy>()) {
        Vec2 onScreen = camera.worldToScreen(enemy->position);
        drawTextCentered("Goblin", onScreen - Vec2{0, 30}, 16);
    }
}
```

---

## Particles

**Particles** are lots of tiny dots that fly out, fade and shrink away:
explosions, sparks, dust, magic, confetti. Every scene has a particle system
called `particles`. It's updated and drawn for you (on top of the entities, and
seen through the camera).

### Bursts

A **burst** fires particles in every direction at once:

```cpp
particles.burst(position, Color::Orange);           // a quick explosion (20 particles)
particles.burst(position, Color::Yellow, 50, 300);  // 50 fast sparks
```

```cpp
particles.burst(position, color, count, speed, lifetime, size);
```

| Value | Meaning | If you leave it out |
|---|---|---|
| `position` | Where it starts | (needed) |
| `color` | The particles' color | (needed) |
| `count` | How many particles | `20` |
| `speed` | How fast they fly out, in pixels per second (each one gets a bit of random speed) | `150` |
| `lifetime` | How many seconds they live | `0.6` |
| `size` | How big they are, in pixels | `4` |

### Sprays

A **spray** fires particles in one direction, spread out a bit, like a rocket
flame or a water fountain:

```cpp
particles.spray(position, Color::Orange, 90, 30);   // downwards (90 degrees), spread over 30 degrees
```

```cpp
particles.spray(position, color, direction, spread, count, speed, lifetime, size);
```

`direction` is an angle in degrees (0 = right, 90 = down, 180 = left, 270 or
-90 = up), and `spread` is how wide the fan of particles is, also in degrees.
The rest default to `count = 5`, `speed = 150`, `lifetime = 0.5` and `size = 3`.
Sprays are made to be called **every frame**, for a steady stream.

### Settings

These change how **all** of the scene's particles behave:

| Setting | What it does | Starts as |
|---|---|---|
| `particles.gravity` | Pulls particles down (pixels per second, per second). Try `400` for falling sparks or confetti. | `0` |
| `particles.drag` | Slows them down over time. `0` = never slow down, `3` = stop quickly | `1` |
| `particles.shrink` | Particles get smaller as they die | `true` |
| `particles.fade` | Particles fade out as they die | `true` |

`particles.clear()` removes them all, and `particles.count()` tells you how many
there are.

### One particle, exactly how you want it

```cpp
Particle p;
p.position = {400, 300};
p.velocity = {0, -100};    // floats upwards
p.color = Color::SkyBlue;
p.size = 6;
p.life = p.maxLife = 1.5f; // lives for 1.5 seconds
particles.add(p);
```

`life` is how many seconds it has left, and `maxLife` how long it lives in total
(used for the shrinking and fading).

### Particle ideas

| Effect | Code |
|---|---|
| Explosion | `particles.burst(position, Color::Orange, 40, 250);` plus `camera.shake(8, 0.3f);` |
| Coin sparkle | `particles.burst(coin->position, Color::Gold, 12, 80);` |
| Rocket flame (every frame) | `particles.spray(back, Color::Orange, angle + 180, 30, 2);` |
| Landing dust | `particles.spray(feet, Color::LightGray, -90, 120, 8, 60);` |
| Confetti | `particles.gravity = 300;` then `particles.burst(position, randomColor(), 80, 400, 2.0f, 6);` |

---

## A complete example: a rocket in space

The world is 2000 x 1200 pixels, much bigger than the window. The camera
follows the rocket smoothly, never shows anything outside the world, zooms with
the mouse wheel and shakes when you click to set off fireworks.

```cpp
#include <sprout/sprout.hpp>

using namespace sprout;

const Rect World{0, 0, 2000, 1200}; // the world is much bigger than the window

struct Rocket : Entity {
    float angle = -90; // pointing up

    void start() override {
        position = World.center();
        size = {30, 30};
    }

    void update(float dt) override {
        angle += getAxis("left", "right") * 180 * dt; // turn

        if (isActionDown("up")) {
            velocity += Vec2::fromAngle(angle, 400 * dt); // push forwards
            Vec2 back = position - Vec2::fromAngle(angle, 18);
            scene().particles.spray(back, Color::Orange, angle + 180, 30, 2); // flames!
        }

        position += velocity * dt;
        position = clamp(position, World); // stay inside the world
    }

    void draw() override {
        Vec2 nose = position + Vec2::fromAngle(angle, 20);
        Vec2 left = position + Vec2::fromAngle(angle + 140, 15);
        Vec2 right = position + Vec2::fromAngle(angle - 140, 15);
        drawTriangle(nose, left, right, Color::White);
    }
};

struct SpaceScene : Scene {
    std::vector<Vec2> stars;

    void start() override {
        background = Color{10, 10, 25};
        for (int i = 0; i < 300; i++) stars.push_back(randomPointIn(World));
        spawn<Rocket>();
        camera.setLimits(World); // never show anything outside the world
    }

    void update(float) override {
        if (Rocket* rocket = findFirst<Rocket>()) camera.follow(rocket->position, 5);

        // Zoom with the mouse wheel, but not too far
        camera.zoom = clamp(camera.zoom + mouseWheel() * 0.1f, 0.5f, 3.0f);

        // Click for fireworks where the mouse is in the world
        if (isMousePressed()) {
            particles.burst(mouseWorldPosition(), randomColor(), 40, 250);
            camera.shake(6, 0.2f);
        }
    }

    void draw() override {
        for (Vec2 star : stars) drawCircle(star, 2, Color::White);
        drawRectOutline(World, Color::DarkBlue, 8); // the edge of the world
    }

    void drawUI() override {
        drawText("Left/Right: turn   Up: thrust   Wheel: zoom   Click: fireworks", 10, 10, 20);
    }
};

int main() {
    Game game("Camera", 800, 450);
    game.run<SpaceScene>();
}
```

New things in there:

- `std::vector<Vec2> stars;` is a **vector**: a list that can grow. (Not to be
  confused with a `Vec2`, which is a math vector: an x and a y.)
  `stars.push_back(...)` adds an item to the end of the list, and
  `for (Vec2 star : stars)` goes through every item.
- `Vec2::fromAngle(angle, length)` makes a `Vec2` pointing at an angle. The
  rocket uses it to push forwards in whatever direction it's facing, and to find
  the corners of its triangle. See [Math and random](14-math-and-random.md).
- `clamp(position, World)` keeps a point inside a rectangle.

> **Try it!** Make the rocket slow down by itself (`velocity *= 0.99f;` every
> frame isn't quite right because it ignores `dt`... try
> `velocity = moveToward(velocity, Vec2{}, 50 * dt);` instead). Add asteroids
> that explode into particles when the rocket touches them.

There's a full shooter with explosions and screen shake in
`examples/06_space_shooter`.

**Next:** [Timers](11-timers.md).
