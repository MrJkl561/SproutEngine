# 9. Levels and physics

This page shows how to build levels out of **letters** with a `TileMap`, and how
to make entities **walk, fall, jump and bump into walls** with `moveAndSlide`.

---

## Tile maps: levels made of letters

Lots of 2D games are built from a grid of square **tiles**, like a mosaic or a
sheet of graph paper. In Sprout, you draw that grid with letters:

```cpp
TileMap map{32, {               // every tile is 32 x 32 pixels
    "####################",
    "#..................#",
    "#..P.........C.....#",
    "#......#####.......#",
    "####################",
}};
```

Each character is one tile. **You decide what each letter means.** A common
choice:

| Letter | Meaning |
|---|---|
| `#` | wall or ground |
| `.` or space | empty air (never drawn, unless you give it a color) |
| `P` | where the player starts |
| `C` | a coin |
| `^` | spikes |

The rows don't all have to be the same length (shorter rows are filled with
spaces), but your level is much easier to read if they are.

### Using a tile map in a scene

Make the map a **member** of your scene (a variable inside the struct), then set
it up in `start()` and draw it in `draw()`:

```cpp
struct Level : Scene {
    TileMap map{32, {
        "####################",
        "#..................#",
        "#..P.........C.....#",
        "#......#####.......#",
        "####################",
    }};

    void start() override {
        map.setSolid("#");                   // '#' tiles are solid walls and floors
        map.setColor('#', Color::DarkGray);  // draw '#' as gray squares
        addSolid(map);                       // let entities bump into the solid tiles

        for (Vec2 spot : map.findTiles('P')) spawn<Player>(spot);
        for (Vec2 spot : map.findTiles('C')) spawn<Coin>(spot);
        map.replaceTiles('P', '.');          // the markers have done their job
        map.replaceTiles('C', '.');
    }

    void draw() override {
        map.draw();
    }
};
```

What's happening:

1. `setSolid("#")` says which letters are **solid**. You can list several, like
   `setSolid("#=B")`.
2. `setColor('#', color)` says how to draw a letter. Letters without a color or
   picture are invisible. (Note the single quotes: `'#'` is one character, while
   `"#"` is text.)
3. `addSolid(map)` tells the scene that this map's solid tiles are walls for
   `moveAndSlide`.
4. `findTiles('P')` gives you the **middle** of every `P` tile. That's perfect
   for spawning entities, because an entity's `position` is its middle too.
5. `replaceTiles('P', '.')` turns the markers into empty air.

> **Important:** `addSolid(map)` remembers *where* the map is, it doesn't make a
> copy. So the map must be a **member of your scene**, not a variable that only
> exists inside `start()`. (Otherwise it's gone when `start()` ends!)

### Pictures for tiles

Instead of colors, tiles can use pictures:

```cpp
map.setImage('#', "assets/wall.png");   // the picture is stretched to the tile size
```

Many artists put all their tiles into one big picture called a **tile set**.
Give `setImage` a `Rect` saying which part of the picture to use:

```cpp
// tiles.png has 16 x 16 tiles side by side
map.setImage('G', "assets/tiles.png", Rect{0, 0, 16, 16});   // the 1st tile: grass
map.setImage('D', "assets/tiles.png", Rect{16, 0, 16, 16});  // the 2nd tile: dirt
map.setImage('W', "assets/tiles.png", Rect{0, 16, 16, 16});  // 1st tile of the 2nd row: water
```

The part is stretched to fit the map's tile size, so 16 x 16 pixel art looks
nice and chunky in a map with 32-pixel tiles.

### Loading levels from files

Typing levels into your code is fine for a few small levels. For more, keep each
level in its own text file, like `games/my_game/assets/level1.txt`:

```
####################
#..................#
#..P.........C.....#
#......#####.......#
####################
```

and load it in `start()`:

```cpp
struct Level : Scene {
    TileMap map;

    void start() override {
        map = TileMap::fromFile("assets/level1.txt", 32); // the file, then the tile size
        map.setSolid("#");
        // ...
    }
};
```

Combined with [passing values to scenes](04-scenes.md#giving-values-to-the-next-scene),
one `Level` scene can load every level:

```cpp
map = TileMap::fromFile("assets/level" + std::to_string(number) + ".txt", 32);
```

If the file can't be found, Sprout prints a warning and you get an empty map.

### Reading and changing tiles

Columns are counted from the left and rows from the top, both starting at 0.

| Code | What it does |
|---|---|
| `getTile(column, row)` | The letter at that spot (a space `' '` outside the map) |
| `setTile(column, row, 'X')` | Changes a tile |
| `tileAt(point)` | The letter under a point in the world (like a position or the mouse) |
| `setTileAt(point, 'X')` | Changes the tile under a point |
| `coordAt(point)` | Which column and row a point is in (`.column` and `.row`) |
| `tileCenter(column, row)` | The middle of a tile, in world coordinates |
| `tileRect(column, row)` | The rectangle a tile covers |
| `findTiles('C')` | The middle of every `C` tile |
| `replaceTiles('a', 'b')` | Turns every `a` into `b` |
| `countTiles('C')` | How many `C` tiles there are |
| `isSolid('#')`, `isSolidAt(point)` | Is that letter, or the tile under that point, solid? |
| `addSolid('X')` | Makes one more letter solid |
| `columns()`, `rows()`, `tileSize()` | The size of the map, in tiles, and the size of one tile |
| `width()`, `height()`, `bounds()` | The size of the map in pixels, and as a `Rect` |
| `position` | Where the map's top-left corner is in the world (`{0, 0}` unless you move it) |

Some ideas:

```cpp
// Spikes: did the player walk into a '^' tile?
if (map.tileAt(player->position) == '^') restartScene();

// Break blocks by clicking on them
if (isMousePressed() && map.tileAt(mouseWorldPosition()) == '#') {
    map.setTileAt(mouseWorldPosition(), '.');
}
```

Changing tiles also changes the walls straight away, because the scene looks at
the map itself.

---

## Physics: walking, falling and jumping

**Physics** in a game means making things move in a believable way: falling,
landing, bumping into walls. Sprout keeps it simple: entities are boxes (their
hitbox), and walls are boxes too.

### `velocity` and `moveAndSlide`

Every entity has a `velocity`: how fast it's moving, in pixels per second, as a
`Vec2` (x = sideways, y = up and down). Instead of changing `position`
yourself, you set the velocity and call `moveAndSlide(dt)`:

```cpp
void update(float dt) override {
    velocity.x = getAxis("left", "right") * 250;
    moveAndSlide(dt);
}
```

`moveAndSlide(dt)` moves the entity by `velocity * dt`, **but stops it at
anything solid**: the solid tiles of maps you added with `addSolid(map)`, and
rectangles you added with `addSolid(rect)`. When it hits something, that part of
the velocity becomes 0, and the entity **slides** along the wall instead of
getting stuck. Fast entities can't skip through thin walls either.

After `moveAndSlide`, you can ask what happened:

| Code | True when the entity... |
|---|---|
| `isOnFloor()` | is standing on something |
| `isOnWall()` | is touching a wall on its left or right |
| `isOnCeiling()` | bumped its head |

`moveAndSlide` also gives back a `MoveResult` with `hitLeft`, `hitRight`,
`hitTop`, `hitBottom`, `hitWall()` and `hitAnything()` if you need the details:

```cpp
MoveResult hit = moveAndSlide(dt);
if (hit.hitLeft) { /* bumped into something on the left */ }
```

### Gravity and jumping

**Gravity** makes things fall faster and faster. Every frame, it adds a bit more
downward speed. **Jumping** sets the vertical speed to a big *negative* number
(remember, negative y is up!), and gravity slowly turns it around:

```
velocity.y:  -650  -625  -600 ...  -25   0   +25 ...  +600
             going up fast   slowing down   top!   falling faster and faster
```

```cpp
const float Gravity = 1500;  // how much faster you fall every second
const float JumpSpeed = 650; // how fast you shoot up when you jump
const float RunSpeed = 250;

struct Player : Entity {
    void update(float dt) override {
        velocity.x = getAxis("left", "right") * RunSpeed;
        velocity.y += Gravity * dt; // gravity pulls down a little more every frame

        if (isOnFloor() && isActionPressed("jump")) {
            velocity.y = -JumpSpeed; // negative = up!
        }

        moveAndSlide(dt);
    }
};
```

`isOnFloor()` makes sure you can only jump while standing on something, so no
jumping in mid-air. (It tells you what happened in the *last* `moveAndSlide`,
which is exactly what we need here.)

`const` means "this never changes". Writing the numbers at the top, with names,
makes them easy to find and tweak.

> **Tweak the feel!** `Gravity` and `JumpSpeed` together decide how high you jump
> and how floaty it feels. Try `Gravity = 3000` with `JumpSpeed = 900` for a snappy
> jump, or `Gravity = 600` with `JumpSpeed = 400` for a moon jump.

**Variable jump height** (tap for a small jump, hold for a big one), as in
many famous platformers:

```cpp
if (isActionReleased("jump") && velocity.y < 0) {
    velocity.y *= 0.5f; // let go early = stop rising sooner
}
```

### Top-down games

For games seen from above (like old Zelda or Pokémon games), there's no
gravity. Just set the whole velocity from the arrow keys:

```cpp
void update(float dt) override {
    velocity = getVector("left", "right", "up", "down") * 200;
    moveAndSlide(dt);
}
```

### Solid rectangles without a tile map

You don't need a tile map to have walls. `addSolid(rect)` makes any rectangle
solid:

```cpp
void start() override {
    addSolid(Rect{0, 420, 800, 30});   // the ground
    addSolid(Rect{300, 320, 150, 20}); // a floating platform
}

void draw() override {
    drawRect(Rect{0, 420, 800, 30}, Color::DarkGreen);
    drawRect(Rect{300, 320, 150, 20}, Color::Brown);
}
```

(Solids are invisible, so draw them yourself.) `clearSolids()` removes all
solid rectangles and maps from the scene.

---

## A complete example: a little platformer

Run and jump with the arrow keys and Space. Collect the coins and avoid the
spikes.

```cpp
#include <sprout/sprout.hpp>

using namespace sprout;

const float Gravity = 1500;  // how fast you fall faster (pixels per second, per second)
const float JumpSpeed = 650; // how fast you shoot up when you jump
const float RunSpeed = 250;  // pixels per second

struct Player : Entity {
    Player(Vec2 where) {
        position = where;
        size = {24, 30};
    }

    void update(float dt) override {
        velocity.x = getAxis("left", "right") * RunSpeed;
        velocity.y += Gravity * dt; // gravity pulls down a little more every frame

        if (isOnFloor() && isActionPressed("jump")) {
            velocity.y = -JumpSpeed; // negative = up!
        }

        moveAndSlide(dt); // move, but stop at walls, floors and ceilings
    }

    void draw() override {
        drawRect(bounds(), Color::SkyBlue);
    }
};

struct Coin : Entity {
    Coin(Vec2 where) {
        position = where;
        size = {16, 16};
    }

    void draw() override {
        drawCircle(position, 8, Color::Gold);
    }
};

struct Level : Scene {
    TileMap map{32, {
        "#########################",
        "#.......................#",
        "#.......................#",
        "#..C.C.C.........C.C.C..#",
        "#.#######.......#######.#",
        "#.......................#",
        "#..........C.C..........#",
        "#........#######........#",
        "#.......................#",
        "#...C.C...........C.C...#",
        "#..######.......######..#",
        "#.P.........^^^.........#",
        "#########################",
        "#########################",
    }};
    int score = 0;

    void start() override {
        background = Color{30, 30, 45};

        map.setSolid("#");                   // '#' tiles are walls and floors
        map.setColor('#', Color::DarkGray);  // ...drawn as gray squares
        addSolid(map);                       // entities using moveAndSlide() bump into them

        for (Vec2 spot : map.findTiles('P')) spawn<Player>(spot);
        for (Vec2 spot : map.findTiles('C')) spawn<Coin>(spot);
        map.replaceTiles('P', '.'); // the markers have done their job
        map.replaceTiles('C', '.');
    }

    void update(float) override {
        Player* player = findFirst<Player>();

        if (Coin* coin = player->findOverlapping<Coin>()) {
            coin->destroy();
            score += 1;
        }

        if (map.tileAt(player->position) == '^') restartScene(); // ouch, spikes!
    }

    void draw() override {
        map.draw();

        // Draw a red triangle on every spike tile
        for (Vec2 spike : map.findTiles('^')) {
            drawTriangle(spike + Vec2{-16, 16}, spike + Vec2{0, -12}, spike + Vec2{16, 16}, Color::Red);
        }
    }

    void drawUI() override {
        drawText("Coins: " + std::to_string(score), 40, 40, 24, Color::Gold);
        if (count<Coin>() == 0) drawTextCentered("You got them all!", screenCenter(), 50);
    }
};

int main() {
    Game game("Platformer", 800, 450);
    game.run<Level>();
}
```

> **Try it!**
> 1. Design your own level. Keep the rows the same length so it's easy to read.
> 2. Add a door tile `D`. When the player touches it, go to the next level (see
>    [Scenes](04-scenes.md#giving-values-to-the-next-scene)).
> 3. Make the level wider than the screen and let the camera follow the player.
>    See [Camera and effects](10-camera-and-effects.md).
> 4. Add `particles.burst(coin->position, Color::Gold);` when collecting a coin.

There's a bigger platformer in `examples/07_platformer`.

---

## Advanced: `moveAndCollide`

`moveAndSlide` is built on a lower-level function that works with any `Rect`,
not just entities. You can use it for things that aren't entities, or to move
something against a different set of walls:

```cpp
Rect box{100, 100, 32, 32};
std::vector<Rect> walls{Rect{0, 400, 800, 50}, Rect{300, 300, 100, 20}};

MoveResult hit = moveAndCollide(box, velocity * dt, walls); // moves `box` itself
if (hit.hitBottom) { /* landed on something */ }

MoveResult hit2 = moveAndCollide(box, velocity * dt, map);  // or against a tile map
```

Unlike `moveAndSlide`, it doesn't change any velocity for you: it just moves the
box as far as it can and tells you what it hit.

---

## Quick reference

| Code | What it does |
|---|---|
| `TileMap map{32, {"###", "#.#"}};` | A map with 32-pixel tiles |
| `TileMap::fromFile("assets/level1.txt", 32)` | A map from a text file |
| `map.setSolid("#=")`, `map.addSolid('X')` | Which letters are solid |
| `map.setColor('#', color)` | Draw a letter as a colored square |
| `map.setImage('#', "assets/wall.png")` | Draw a letter as a picture |
| `map.setImage('#', "assets/tiles.png", Rect{x, y, w, h})` | ...using part of a tile set |
| `map.draw()` | Draws the map (in `draw()`) |
| `map.findTiles('P')`, `map.replaceTiles('P', '.')` | Markers for spawning |
| `addSolid(map)`, `addSolid(rect)`, `clearSolids()` | Walls for `moveAndSlide` (in a scene) |
| `velocity`, `moveAndSlide(dt)` | Moving with walls |
| `isOnFloor()`, `isOnWall()`, `isOnCeiling()` | What did the last `moveAndSlide` touch? |
| `moveAndCollide(rect, motion, walls or map)` | Advanced: move any `Rect` |

**Next:** [Camera and effects](10-camera-and-effects.md).
