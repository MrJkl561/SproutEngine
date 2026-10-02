# 11. Timers: waiting for things

Games are full of waiting:

- "Spawn an enemy **every** 2 seconds."
- "Go to the game-over screen **2 seconds after** the player dies."
- "The player can shoot **at most** 4 times a second."
- "The player is invincible **for** 3 seconds after being hit."
- "You have **60 seconds** to finish the level."

You *could* count seconds yourself with `dt`, but Sprout has four ready-made
tools that make it much easier:

| Tool | Answers the question | Example |
|---|---|---|
| `after(seconds, ...)` | "Do this once, later" | Game over 2 seconds after dying |
| `every(seconds, ...)` | "Do this again and again" | A new enemy every 1.5 seconds |
| `Timer` | "Has enough time passed yet?" | A 60-second round |
| `Cooldown` | "Am I allowed to do this again yet?" | Shooting, dashing |

All of them use **game time**: they stop counting while the scene is
[paused](04-scenes.md#pausing).

---

## `after` and `every`

These belong to the scene. You give them a number of seconds and some code to
run:

```cpp
void start() override {
    // Once, 2 seconds from now:
    after(2.0f, [this] { spawn<Boss>(); });

    // Again and again, every 1.5 seconds:
    every(1.5f, [this] { spawn<Enemy>(); });
}
```

The code between `{` and `}` doesn't run straight away. It's saved and run
later, when the time comes.

### What's this `[this] { ... }` thing?

It's called a **lambda**. A lambda is a little packet of code that you can hand
to someone else to run later, like writing instructions on a sticky note and
giving it to a friend: "In 2 seconds, spawn the boss."

```
  [this]     {  spawn<Boss>();  }
  ──────     ─────────────────────
  what the   the code to run
  code may
  use
```

The part in square brackets is the **capture list**. It says which outside
things the code is allowed to use. Because the sticky note is read later, it
needs permission to reach things:

- `[this]` means "you may use **this scene**": its functions (like `spawn`,
  `particles`, `camera`) and its variables (like `score`). This is the one you'll
  use almost every time.
- `[]` means "nothing from outside". It's enough when the code only uses free
  functions like `changeScene` or `playSound`:

  ```cpp
  after(2.0f, [] { changeScene<GameOver>(); });
  ```

- `[this, where]` also takes a **copy** of a local variable called `where`.
  You need this for variables that only exist inside the function you're in,
  because by the time the lambda runs, that function has finished and its local
  variables are gone:

  ```cpp
  void explodeLater(Vec2 where) {
      after(0.5f, [this, where] { particles.burst(where, Color::Red); });
  }
  ```

If you forget `[this]` and use something from the scene, you get an error like
*"'this' was not captured for this lambda function"*. Just add `this` between
the brackets.

### Cancelling

`after` and `every` give back an **id number**. Keep it if you want to stop that
timer later:

```cpp
int spawner = 0; // a member of the scene

void start() override {
    spawner = every(2.0f, [this] { spawn<Enemy>(); });
}

void update(float dt) override {
    if (count<Enemy>() > 20) cancelTimer(spawner);   // stop spawning
}
```

`cancelAllTimers()` stops every `after` and `every` in the scene. When the
scene ends (`changeScene`, `restartScene`), all its timers end with it, so you
never have to clean them up yourself.

### Good to know

- `after` and `every` run right after your scene's `update()`, before the
  entities update. So an entity you spawn in them starts moving straight away.
- Inside an **entity**, use `scene().after(...)`. But be careful with `[this]`
  there: if the entity is destroyed before the time is up, the lambda would use a
  dead entity. Inside entities, a `Timer` or `Cooldown` member is safer (see
  below), because it disappears together with the entity.

---

## `Timer`: has enough time passed?

A `Timer` starts counting as soon as it's made. Ask it `isDone()` whenever you
like:

```cpp
struct PlayScene : Scene {
    Timer spawnTimer{2.0f}; // 2 seconds, starts counting right away

    void update(float dt) override {
        if (spawnTimer.isDone()) {
            spawn<Enemy>();
            spawnTimer.restart(); // wait another 2 seconds
        }
    }
};
```

Unlike an alarm clock, a timer doesn't *do* anything when it's done. It just
answers "yes" from then on, until you `restart()` it. You don't need to update
it, either.

| Code | What it does |
|---|---|
| `Timer name{seconds};` | A timer that is done after that many seconds |
| `isDone()` | True once the time is up |
| `restart()` | Starts counting again from zero |
| `restart(seconds)` | Starts again, with a new length |
| `timeLeft()` | Seconds left (0 when done) |
| `elapsed()` | Seconds since it (re)started |
| `progress()` | From 0.0 (just started) to 1.0 (done). Great for progress bars! |
| `duration()` | How long the timer is |

> **Use curly brackets** for timers that are members of a struct:
> `Timer spawnTimer{2.0f};`. Round brackets `Timer spawnTimer(2.0f);` don't work
> there, and give a confusing error like *"expected identifier before numeric
> constant"*.

Some ideas:

```cpp
// A 60-second round with a countdown and a bar
Timer roundTimer{60.0f};

void update(float dt) override {
    if (roundTimer.isDone()) changeScene<GameOver>(score);
}

void drawUI() override {
    drawText(std::to_string(int(roundTimer.timeLeft())), 10, 40, 20);
    progressBar(Rect{10, 10, 200, 20}, roundTimer.progress());
}
```

```cpp
// Spawning faster and faster: each wait is 10% shorter than the last
if (spawnTimer.isDone()) {
    spawn<Enemy>();
    spawnTimer.restart(spawnTimer.duration() * 0.9f);
}
```

```cpp
// Invincible (and blinking) for 2 seconds after being hit
struct Player : Entity {
    Timer invincible{0.0f}; // 0 seconds = already done = not invincible

    void update(float dt) override {
        if (findOverlapping<Enemy>() && invincible.isDone()) {
            // ...lose a life...
            invincible.restart(2.0f);
        }
        // Blink: visible for a tenth of a second, invisible for the next tenth...
        if (!invincible.isDone()) visible = int(invincible.elapsed() * 10) % 2 == 0;
        else visible = true;
    }
};
```

(`%` gives the remainder of a division, so `something % 2 == 0` is true for
even numbers and false for odd ones: perfect for blinking.)

---

## `Cooldown`: am I allowed yet?

A `Cooldown` is for things the player can do, but not *too* often: shooting,
dashing, using a potion.

```cpp
struct Player : Entity {
    Cooldown gun{0.25f}; // at most 4 shots per second

    void update(float dt) override {
        if (isActionDown("shoot") && gun.use()) {
            scene().spawn<Bullet>(position);
        }
    }
};
```

`gun.use()` does two things in one go: **if** the cooldown is ready, it starts
the cooldown and gives `true`. If it isn't ready yet, it gives `false` and
nothing happens. So holding the shoot button fires one bullet every 0.25
seconds.

A cooldown **starts ready**, so the very first shot happens instantly.

| Code | What it does |
|---|---|
| `Cooldown name{seconds};` | Can be used once every that many seconds |
| `use()` | If ready: starts the cooldown and gives `true`. Otherwise gives `false`. |
| `isReady()` | Would `use()` work right now? (Doesn't start anything.) |
| `reset()` | Makes it ready immediately (a power-up!) |
| `timeLeft()` | Seconds until it's ready again |
| `duration()` | How long the cooldown is |
| `setDuration(seconds)` | Changes how long the cooldown is (a "rapid fire" upgrade) |

Showing a cooldown as a little bar under the player:

```cpp
Cooldown dash{1.0f};

void draw() override {
    float ready = 1 - dash.timeLeft() / dash.duration(); // 0 = just used, 1 = ready
    drawRect(Rect{position.x - 20, position.y - 30, 40 * ready, 4}, Color::Green);
}
```

---

## Which one should I use?

- Something should **happen** at a certain time, once or repeatedly, and it
  belongs to the scene? Use **`after`** or **`every`**.
- You want to **ask** "is it time yet?" in your `update()`, or the waiting
  belongs to an **entity**? Use a **`Timer`**.
- The player wants to do something and you need to say "not yet"? Use a
  **`Cooldown`**.

**Next:** [Menus and UI](12-menus-and-ui.md).
