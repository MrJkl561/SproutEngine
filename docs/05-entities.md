# 5. Entities

An **entity** is a "thing" in your game: the player, an enemy, a coin, a
bullet, a cloud drifting by. If it moves, gets hit or gets collected, it's
probably an entity.

The scene looks after its entities for you. Every frame it calls their
`update()` and `draw()`, and when they're destroyed, it cleans them up.

---

## Making an entity

Make a struct that inherits from `Entity` and fill in the parts you need:

```cpp
struct Coin : Entity {
    Coin(Vec2 where) {          // constructor: runs when the coin is made
        position = where;
        size = {16, 16};
    }

    void update(float dt) override {
        // runs every frame
    }

    void draw() override {
        drawCircle(position, 8, Color::Gold);
    }
};
```

Then put one in your scene, usually in the scene's `start()` or `update()`:

```cpp
spawn<Coin>(Vec2{100, 200});
```

### What every entity has

| Member | What it is | Starts as |
|---|---|---|
| `position` | Where the **middle** of the entity is, in world coordinates | `{0, 0}` |
| `size` | Width and height of the **hitbox** (the invisible box used for collisions) | `{32, 32}` |
| `velocity` | Speed in pixels per second. Used by `moveAndSlide()`, and you can use it yourself too | `{0, 0}` |
| `layer` | Higher layers are drawn on top of lower ones | `0` |
| `visible` | `false` hides the entity (it still updates) | `true` |

> **Remember:** `position` is the **middle**, not the top-left corner. That makes
> `drawCircle(position, ...)` and `drawTextureCentered(..., position)` just work.
> When you need a rectangle, `bounds()` gives you the hitbox as a `Rect`, so
> `drawRect(bounds(), color)` draws exactly the hitbox.

### The functions you can fill in

| Function | When it runs |
|---|---|
| Constructor, e.g. `Coin(Vec2 where)` | When the entity is made. Give it values here. |
| `void start() override` | Right after it's added to the scene. `scene()` works here. |
| `void update(float dt) override` | Every frame (unless the scene is paused) |
| `void draw() override` | Every frame, if `visible` is true |

**Constructor or `start()`?** Use the constructor for values that are passed in
(like where to appear). Use `start()` for anything that needs the scene, such as
finding the player. Inside the constructor the entity isn't in a scene yet, so
`scene()` doesn't work there (Sprout stops with a message telling you to move
that code into `start()`).

---

## Spawning

`spawn<Type>(values...)` makes a new entity and adds it to the scene. The values
are handed to the entity's constructor:

```cpp
struct Bullet : Entity {
    Bullet(Vec2 start, Vec2 speed) {
        position = start;
        velocity = speed;
        size = {6, 14};
    }

    void update(float dt) override {
        position += velocity * dt;
        if (position.y < -20) destroy(); // flew off the top of the screen
    }

    void draw() override {
        drawRect(bounds(), Color::Yellow);
    }
};

// in the scene:
spawn<Bullet>(Vec2{400, 400}, Vec2{0, -500}); // from (400,400), going up at 500 pixels/second
```

> **Write `Vec2{...}`, not just `{...}`, inside `spawn`.** `spawn<Coin>({100, 100})`
> gives a confusing "no matching function" error, because C++ can't guess what
> type `{100, 100}` is meant to be when it's passed through `spawn`. Writing
> `spawn<Coin>(Vec2{100, 100})` fixes it. The same goes for `changeScene` and
> `game.run`.

### Changing it after spawning

`spawn` gives back the new entity, so you can change it straight away:

```cpp
Bullet& bullet = spawn<Bullet>(Vec2{400, 400}, Vec2{0, -500});
bullet.layer = 5;
```

The `&` in `Bullet&` means "the actual bullet, not a copy of it". (Note that the
entity's `start()` has already run by the time `spawn` gives it back.)

### Spawning from inside an entity

`spawn` belongs to the scene. Inside an entity, reach the scene with `scene()`:

```cpp
struct Ship : Entity {
    Cooldown gun{0.2f}; // at most 5 shots per second

    void update(float dt) override {
        if (isActionDown("jump") && gun.use()) {
            scene().spawn<Bullet>(position, Vec2{0, -500});
        }
    }
};
```

The same goes for the scene's particles and camera:
`scene().particles.burst(position, Color::Orange);` and
`scene().camera.shake(5, 0.2f);`.

---

## Destroying

```cpp
destroy();          // inside the entity: remove myself
coin->destroy();    // from outside: remove that coin
```

A destroyed entity disappears at the end of the frame. From the moment you call
`destroy()`, `findAll`, `findFirst`, `count` and the other find functions
ignore it. `isDestroyed()` tells you if it has been destroyed.

---

## Finding entities

These work inside a scene. Inside an entity, put `scene().` in front, like
`scene().findFirst<Player>()`.

| Code | What you get |
|---|---|
| `findAll<Enemy>()` | A list of every Enemy |
| `findFirst<Player>()` | The first Player, or `nullptr` if there is none |
| `findClosest<Enemy>(point)` | The Enemy nearest to a point, or `nullptr` |
| `count<Coin>()` | How many Coins there are |
| `destroyAll<Bullet>()` | Destroys every Bullet |
| `findAll()`, `count()`, `destroyAll()` | Leave out `<...>` to mean **every** entity |

The find functions give you **pointers** (`Enemy*`). A pointer is like the
address of a house: it tells you where the entity lives. Use `->` instead of `.`
to reach its parts:

```cpp
for (Enemy* enemy : findAll<Enemy>()) {
    enemy->position.y += 10;
}
```

`nullptr` means "nothing" (an empty address). `findFirst` and `findClosest` give
`nullptr` when there's nothing to find, so **always check before you use it**:

```cpp
Player* player = findFirst<Player>();
if (player) {
    camera.follow(player->position);
}
```

### Kinds of entities

`findAll<T>()` also finds entities of types that inherit from `T`. If `Slime`
and `Bat` both inherit from `Enemy`, then `findAll<Enemy>()` finds slimes **and**
bats:

```cpp
struct Enemy : Entity { int health = 3; };
struct Slime : Enemy { /* ... */ };
struct Bat : Enemy { /* ... */ };
```

---

## Collisions

Two entities **collide** when their hitboxes overlap.

| Code | What it tells you |
|---|---|
| `overlaps(other)` | Does my hitbox touch this other entity's? |
| `overlaps(rect)` | Does my hitbox touch this rectangle? |
| `bounds()` | My hitbox as a `Rect` |
| `findOverlapping<Coin>()` | The first Coin touching me, or `nullptr` |
| `findAllOverlapping<Enemy>()` | Every Enemy touching me |
| `bounds().contains(point)` | Is a point (like the mouse) inside me? |

Collecting coins from inside the player:

```cpp
void update(float dt) override {
    if (Coin* coin = findOverlapping<Coin>()) {
        coin->destroy();
    }
}
```

`if (Coin* coin = ...)` finds a coin **and** checks it isn't `nullptr`, in one
line. The code inside only runs when there really is a coin.

Bullets hitting enemies, checked in the scene:

```cpp
void update(float dt) override {
    for (Enemy* enemy : findAll<Enemy>()) {
        for (Bullet* bullet : findAll<Bullet>()) {
            if (bullet->overlaps(*enemy)) {
                bullet->destroy();
                enemy->health -= 1;
                if (enemy->health <= 0) enemy->destroy();
            }
        }
    }
}
```

(`*enemy` turns the pointer back into the enemy itself, because `overlaps` wants
the entity, not its address.)

Clicking on an entity:

```cpp
if (isMousePressed() && bounds().contains(mouseWorldPosition())) {
    // I was clicked!
}
```

> **Round things:** hitboxes are always rectangles. For circles, check the
> distance instead: `if (position.distanceTo(other->position) < 20 + 30)`
> (the two radiuses added together).

For walls and floors that entities shouldn't walk through, use
`moveAndSlide()`. See [Levels and physics](09-levels-and-physics.md).

---

## Layers: what's drawn on top

Entities with a higher `layer` are drawn on top of entities with a lower one.
Entities on the same layer are drawn in the order they were spawned.

```cpp
struct Cloud : Entity {
    Cloud() { layer = 10; } // clouds float above everything
};
struct Puddle : Entity {
    Puddle() { layer = -1; } // puddles go under everything
};
```

The scene's own `draw()` always comes first (underneath all entities), particles
come after the entities, and `drawUI()` is on top of everything.

---

## A complete example

A player collects coins while a ghost slowly hunts them down. Press **C** to
clear the coins.

```cpp
#include <sprout/sprout.hpp>

using namespace sprout;

// The player: a blue square you move with the arrow keys or WASD.
struct Player : Entity {
    void start() override {
        position = screenCenter();
        size = {30, 30};
        layer = 2; // drawn on top of everything else
    }

    void update(float dt) override {
        position += getVector("left", "right", "up", "down") * 250 * dt;
    }

    void draw() override {
        drawRect(bounds(), Color::SkyBlue);
    }
};

// A coin that just sits there, waiting to be collected.
struct Coin : Entity {
    Coin(Vec2 where) {
        position = where;
        size = {16, 16};
    }

    void draw() override {
        drawCircle(position, 8, Color::Gold);
    }
};

// A ghost that slowly floats towards the player.
struct Ghost : Entity {
    float speed;

    Ghost(Vec2 where, float howFast) {
        position = where;
        speed = howFast;
        size = {28, 28};
        layer = 1;
    }

    void update(float dt) override {
        Player* player = scene().findFirst<Player>();
        if (player) position = moveToward(position, player->position, speed * dt);
    }

    void draw() override {
        drawCircle(position, 14, Color::White.withAlpha(0.7f));
    }
};

struct CoinScene : Scene {
    int score = 0;

    void start() override {
        background = Color{30, 30, 40};
        spawn<Player>();
        spawn<Ghost>(Vec2{50, 50}, 60.0f);

        // A new coin every second, but never more than 10 at once
        every(1.0f, [this] {
            if (count<Coin>() < 10) spawn<Coin>(randomPointIn(Rect{20, 20, 760, 410}));
        });
    }

    void update(float) override {
        Player* player = findFirst<Player>();

        if (Coin* coin = player->findOverlapping<Coin>()) {
            coin->destroy();
            score += 1;
            particles.burst(coin->position, Color::Gold, 15);
        }

        if (player->findOverlapping<Ghost>()) restartScene(); // caught!

        if (isKeyPressed(Key::C)) destroyAll<Coin>(); // C clears all coins
    }

    void drawUI() override {
        drawText("Score: " + std::to_string(score), 10, 10, 24);
        drawText("Coins: " + std::to_string(count<Coin>()), 10, 40, 20, Color::LightGray);
    }
};

int main() {
    Game game("Coins and a ghost", 800, 450);
    game.run<CoinScene>();
}
```

`moveToward(from, to, maxStep)` moves a point towards another point, but never
further than `maxStep`. It's perfect for chasing. See
[Math and random](14-math-and-random.md).

> **Try it!** Spawn a second, faster ghost after 10 seconds with
> `after(10.0f, [this] { spawn<Ghost>(Vec2{750, 50}, 100.0f); });`. Make the
> player flash by switching `visible` on and off.

---

## Don't hold on to entity pointers

This is the one rule that can crash your game, so read it carefully.

A pointer is just an address. When an entity is destroyed, it's removed at the
end of the frame and the memory it lived in is given to something else. If you
kept its address, that address now points at garbage. Using it can crash the
game, or worse, cause weird bugs that are very hard to find.

**Risky.** This keeps a pointer in a member variable for a long time:

```cpp
struct Turret : Entity {
    Enemy* target = nullptr; // DANGER: this enemy might get destroyed later

    void start() override {
        target = scene().findFirst<Enemy>();
    }

    void update(float dt) override {
        // If the enemy was destroyed in an earlier frame, this reads garbage!
        float distance = position.distanceTo(target->position);
    }
};
```

**Safe.** Look it up again every frame, and check for `nullptr`:

```cpp
struct Turret : Entity {
    void update(float dt) override {
        Enemy* target = scene().findClosest<Enemy>(position);
        if (target) {
            float distance = position.distanceTo(target->position);
            // ...
        }
    }
};
```

Finding entities is quick, so don't worry about doing it every frame.

The rules of thumb:

- Pointers you get **during a frame** are fine to use **during that frame**,
  even if you call `destroy()` on them.
- **Don't keep them** in member variables for later frames, unless you're sure
  that entity can never be destroyed while it's stored (for example a player that
  only disappears when the whole scene restarts).
- Instead of a pointer, you can store **what you need** (like a position), or
  look the entity up again next frame.

---

## `this`, `[this]` and `scene()`

Inside a struct's functions, `this` means "the object I'm inside of".

- **Inside a scene**, you *are* the scene. You can call `spawn`, `findAll`,
  `after`, `camera` and `particles` directly.
- **Inside an entity**, you're the entity. To reach the scene it lives in, use
  `scene()`: `scene().spawn<Bullet>(...)`, `scene().findFirst<Player>()`,
  `scene().particles.burst(...)`. Entities do have their **own** `after` and
  `every`, though (see below).

**Lambdas** (the `[this] { ... }` bits you give to `after` and `every`) need to
be told which outside things they're allowed to use. `[this]` lets the code
inside use the object you're in:

```cpp
// in a scene: spawn an enemy every 2 seconds
every(2.0f, [this] { spawn<Enemy>(); });
```

Without `[this]`, you'd get the error *"'this' was not captured for this lambda
function"*. Lambdas are explained gently in [Timers](11-timers.md).

### Timers that belong to an entity

Entities have their own `after(seconds, ...)` and `every(seconds, ...)`. They
work just like the scene's, with one big bonus: they **stop by themselves when
the entity is destroyed**. So using `[this]` inside them is always safe, even if
the entity is blown up before the time is up.

```cpp
struct Bomb : Entity {
    void start() override {
        after(2.0f, [this] {                       // BOOM, 2 seconds after being spawned
            scene().particles.burst(position, Color::Orange, 40, 250);
            scene().camera.shake(10, 0.3f);
            destroy();
        });
    }
};

struct Turret : Entity {
    void start() override {
        every(1.0f, [this] { scene().spawn<Bullet>(position); }); // until the turret is destroyed
    }
};
```

Call them in `start()` or `update()`, not in the constructor (the entity
isn't in a scene yet there). They pause along with the scene. Like the scene's
versions they give back an id, and `scene().cancelTimer(id)` stops one early.

Inside an entity, always use the entity's own `after`/`every` rather than
`scene().after(...)`: a scene timer doesn't know about the entity, so if the
entity is destroyed first, `[this]` would point at a dead entity. A `Timer`
member (see [Timers](11-timers.md#timer-has-enough-time-passed)) is another
safe choice:

```cpp
struct Spark : Entity {
    Timer life{0.5f}; // starts counting as soon as the spark is made

    void update(float dt) override {
        if (life.isDone()) destroy();
    }
};
```

---

## Going further: an entity that changes its scene's variables

`scene()` gives you a plain `Scene&`, which doesn't know about *your* scene's
variables like `score`. One solution is to do that work **in the scene**, like
the bullets-and-enemies loop above. The other is `sceneAs<YourScene>()`, which
gives you the scene as **your** scene type, with all its variables.

Because the coin needs `PlayScene` and `PlayScene` spawns coins, they need each
other, so we use the [split-function trick](04-scenes.md#when-two-scenes-need-each-other)
again:

```cpp
struct PlayScene; // "there will be a scene called PlayScene" (written further down)

struct Coin : Entity {
    void update(float dt) override; // body written further down
};

struct PlayScene : Scene {
    int score = 0;
    // ...
};

void Coin::update(float dt) {
    if (Player* player = scene().findFirst<Player>(); player && overlaps(*player)) {
        PlayScene& play = sceneAs<PlayScene>(); // my scene, as a PlayScene
        play.score += 1;
        destroy();
    }
}
```

For a quick one-off, `sceneAs<PlayScene>().score += 1;` works too. If the
coin is ever spawned in a *different* kind of scene, `sceneAs` stops the game
with a clear message ("sceneAs<...>() asked for the wrong kind of scene")
instead of doing something strange.

---

## Quick reference

| Code | What it does |
|---|---|
| `struct Name : Entity { ... };` | Makes a kind of entity |
| `position`, `size`, `velocity`, `layer`, `visible` | What every entity has |
| `start()`, `update(dt)`, `draw()` | Functions you can fill in |
| `spawn<T>(values...)` | Makes an entity (in a scene). Gives back `T&`. |
| `scene()` | The scene this entity is in (not in the constructor) |
| `sceneAs<MyScene>()` | The scene as your own scene type, so you can use its variables |
| `after(seconds, [this] { ... })`, `every(seconds, [this] { ... })` | Entity timers: stop by themselves when the entity is destroyed |
| `destroy()`, `isDestroyed()` | Removing entities |
| `findAll<T>()`, `findFirst<T>()`, `findClosest<T>(point)`, `count<T>()`, `destroyAll<T>()` | Finding entities (in a scene) |
| `bounds()`, `overlaps(...)`, `findOverlapping<T>()`, `findAllOverlapping<T>()` | Collisions |
| `moveAndSlide(dt)`, `isOnFloor()`, `isOnWall()`, `isOnCeiling()` | Moving with walls, see [Levels and physics](09-levels-and-physics.md) |

**Next:** [Input](06-input.md): keyboard, mouse and gamepads.
