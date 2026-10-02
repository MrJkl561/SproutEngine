# 7. Drawing: shapes, colors, text and pictures

All drawing happens in your scene's `draw()` and `drawUI()` functions, or in an
entity's `draw()`.

- `draw()` draws the **game world**. It's seen through the camera, so it scrolls
  and zooms when the camera does.
- `drawUI()` draws **on top of everything** and ignores the camera. Use it for
  scores, health bars and menus.

**Things drawn later appear on top of things drawn earlier**, like painting:
paint the sky first, then the mountains, then the trees in front. (Entities are
sorted by their `layer`, see [Entities](05-entities.md#layers-whats-drawn-on-top).)

---

## Shapes

| Code | Draws |
|---|---|
| `drawRect(x, y, width, height, color)` | A filled rectangle. `x, y` is the **top-left corner**. |
| `drawRect(rect, color)` | The same, using a `Rect` |
| `drawRectOutline(rect, color, thickness = 2)` | Just the border of a rectangle |
| `drawRoundedRect(rect, cornerRadius, color)` | A rectangle with rounded corners |
| `drawCircle(center, radius, color)` | A filled circle around its **middle** |
| `drawCircle(x, y, radius, color)` | The same, with separate x and y |
| `drawCircleOutline(center, radius, color, thickness = 2)` | A ring |
| `drawLine(from, to, color, thickness = 2)` | A line between two points |
| `drawTriangle(a, b, c, color)` | A filled triangle (corners in any order) |
| `drawPolygon(center, sides, radius, color, rotation = 0)` | A regular shape: 3 = triangle, 5 = pentagon, 6 = hexagon... |

`= 2` and `= 0` mean "you can leave this out and it will be 2 (or 0)".

Here's a program that draws one of everything:

```cpp
#include <sprout/sprout.hpp>

using namespace sprout;

struct DrawingDemo : Scene {
    float spin = 0;

    void start() override {
        background = Color::OffWhite;
    }

    void update(float dt) override {
        spin += 90 * dt; // 90 degrees per second
    }

    void draw() override {
        // Shapes
        drawRect(40, 60, 120, 80, Color::Red);                      // filled rectangle
        drawRectOutline(Rect{180, 60, 120, 80}, Color::Blue, 4);    // just the border
        drawRoundedRect(Rect{320, 60, 120, 80}, 20, Color::Green);  // rounded corners
        drawCircle({520, 100}, 40, Color::Orange);                  // filled circle
        drawCircleOutline({640, 100}, 40, Color::Purple, 6);        // a ring
        drawLine({40, 180}, {300, 260}, Color::DarkGray, 5);        // a thick line
        drawTriangle({360, 260}, {420, 170}, {480, 260}, Color::SkyBlue);
        drawPolygon({580, 215}, 6, 45, Color::Gold, spin);          // a spinning hexagon
        drawPolygon({710, 215}, 5, 45, Color::Pink);                // a pentagon

        // A rainbow made with fromHSV (hue goes from 0 to 360)
        for (int i = 0; i < 10; i++) {
            drawRect(40 + i * 72, 290, 64, 40, Color::fromHSV(i * 36, 0.8f, 1.0f));
        }

        // Lighter, darker and see-through
        drawRect(40, 345, 200, 40, Color::Blue.lighter(0.5f));
        drawRect(260, 345, 200, 40, Color::Blue);
        drawRect(480, 345, 200, 40, Color::Blue.darker(0.5f));
        drawRect(150, 320, 420, 30, Color::Black.withAlpha(0.3f)); // 30% solid, on top
    }

    void drawUI() override {
        drawText("Shapes and colors", 10, 10, 30, Color::DarkGray);
        drawTextCentered("This text is centered", {400, 420}, 24, Color::Maroon);
    }
};

int main() {
    Game game("Drawing", 800, 450);
    game.run<DrawingDemo>();
}
```

`for (int i = 0; i < 10; i++) { ... }` is a **loop**: it runs the code inside
10 times, with `i` being 0, then 1, then 2... up to 9. We use `i` to move each
rainbow square 72 pixels further right and to pick a different color.

> **Try it!** Draw a house: a square for the walls, a triangle for the roof, a
> rectangle for the door and two small squares for windows. Then draw a sun with
> `drawCircle` and a few rays with `drawLine`.

---

## Colors

### Ready-made colors

| | | | |
|---|---|---|---|
| `Color::White` | `Color::Black` | `Color::LightGray` | `Color::Gray` |
| `Color::DarkGray` | `Color::Red` | `Color::Maroon` | `Color::Pink` |
| `Color::Orange` | `Color::Gold` | `Color::Yellow` | `Color::Green` |
| `Color::Lime` | `Color::DarkGreen` | `Color::SkyBlue` | `Color::Blue` |
| `Color::DarkBlue` | `Color::Cyan` | `Color::Purple` | `Color::Violet` |
| `Color::DarkPurple` | `Color::Magenta` | `Color::Beige` | `Color::Brown` |
| `Color::DarkBrown` | `Color::OffWhite` | `Color::Blank` (invisible) | `Color::Transparent` (invisible) |

### Making your own

Screens mix every color from **red, green and blue** light. Each goes from 0
(none) to 255 (full):

```cpp
Color{255, 0, 0}         // pure red
Color{255, 255, 0}       // red + green light = yellow!
Color{20, 20, 50}        // a dark night blue
Color{255, 128, 0, 100}  // orange, mostly see-through (the 4th number is "alpha")
```

**Alpha** is how solid a color is: 0 is invisible and 255 is completely solid.

Other ways to make colors:

| Code | What you get |
|---|---|
| `Color::fromHex(0xFF8800)` | A color written like on websites (`#FF8800`). Handy for copying colors from a color picker. |
| `Color::fromHSV(hue, saturation, brightness)` | Hue is the position on the rainbow (0 = red, 120 = green, 240 = blue, 360 = red again). Saturation and brightness go from 0 to 1. |
| `color.withAlpha(0.5f)` | The same color, half see-through (0.0 = invisible, 1.0 = solid) |
| `color.lighter(0.3f)` | A bit lighter (0 = no change, 1 = white) |
| `color.darker(0.3f)` | A bit darker (0 = no change, 1 = black) |
| `Color::mix(a, b, 0.5f)` | Halfway between two colors |
| `randomColor()` | A random bright color |

A fun one: `Color::fromHSV(timeSinceStart() * 100, 1, 1)` slowly cycles through
the rainbow.

---

## Text

| Code | What it does |
|---|---|
| `drawText(text, x, y, size, color = White)` | Draws text with its **top-left corner** at x, y |
| `drawText(text, position, size, color = White)` | The same, with a `Vec2` |
| `drawTextCentered(text, center, size, color = White)` | Puts the **middle** of the text at `center` |
| `measureText(text, size)` | How wide the text will be, in pixels |
| `drawFPS(x = 10, y = 10)` | Shows the frames per second |

`size` is the height of the letters in pixels. 20 is small, 40 is a heading and
80 is a big title.

To show numbers, turn them into text with `std::to_string` and glue pieces
together with `+`:

```cpp
drawText("Score: " + std::to_string(score), 10, 10, 30);
drawText("Level " + std::to_string(level) + " of 10", 10, 50, 20, Color::Gold);
```

> **Decimal numbers:** `std::to_string(3.5f)` gives `"3.500000"`. For a whole
> number, use `std::to_string(int(value))`, which chops off the decimals.

Right-aligning text (for example a score in the top-right corner) is a job for
`measureText`:

```cpp
std::string text = "Score: " + std::to_string(score);
drawText(text, screenWidth() - measureText(text, 30) - 10, 10, 30);
```

### Fonts

Sprout's built-in font is a chunky pixel font. To use a different font, put a
`.ttf` or `.otf` file in your game's `assets` folder and call:

```cpp
useFont("assets/myfont.ttf");
```

From then on, all `drawText` and `drawTextCentered` calls use that font. Call it
once, for example in your first scene's `start()`. `useDefaultFont()` switches
back to the pixel font.

`useFont` has an optional second number, `quality` (64 if you leave it out).
It's the size the letters are prepared at. If your text is bigger than that and
looks blurry, use a bigger number: `useFont("assets/title.ttf", 128);`.

> The buttons and sliders from [Menus and UI](12-menus-and-ui.md) keep their own
> style and don't use `useFont`.

**Free fonts:** [Google Fonts](https://fonts.google.com) and
[Kenney's fonts](https://kenney.nl/assets/kenney-fonts) are free to use in your
games. Always check the license that comes with a font.

---

## Pictures (textures)

A picture loaded into the graphics card is called a **texture**.

### Where to put your pictures

Put them in an `assets` folder **next to your `main.cpp`**:

```
games/
└── my_game/
    ├── main.cpp
    └── assets/
        ├── player.png
        └── coin.png
```

Every time you build, Sprout copies the `assets` folder next to your game's
program, and makes your game start in that folder. So the path is always just
`"assets/player.png"`, no matter where the game is started from.

> **Spelling matters!** `"assets/Player.png"` and `"assets/player.png"` are
> different files on macOS and Linux. If a picture can't be found, Sprout prints
> a warning in the terminal and draws a **pink and black checkerboard** instead,
> so you can see which picture is missing.

### Loading and drawing

```cpp
Texture coin = loadTexture("assets/coin.png");

drawTexture(coin, {20, 80});                     // top-left corner at (20, 80)
drawTextureCentered(coin, {100, 88});            // middle at (100, 88)
drawTextureCentered(coin, {160, 88}, 2.0f);      // twice as big
drawTextureCentered(coin, {220, 88}, 2.0f, 45);  // twice as big, turned 45 degrees
drawTextureCentered(coin, {280, 88}, 1.0f, 0, Color::Red);  // tinted red
```

- `loadTexture(path)` loads a picture (.png, .jpg, .bmp, ...). Loading the same
  file again is free: you get the same picture back, so you can even call it
  inside `draw()`.
- Pictures can only be loaded **after the `Game` is created** (the graphics card
  isn't ready before that). Loading in `start()`, `update()` or `draw()` is fine.
- A `Texture` is just a small "ticket" for the picture. Copying it is cheap.
  `texture.width`, `texture.height` and `texture.size()` tell you how big it is.

| Code | What it does |
|---|---|
| `drawTexture(texture, position, tint = White)` | Draws the whole picture with its top-left corner at `position` |
| `drawTextureCentered(texture, center, scale = 1, rotation = 0, tint = White)` | Draws it with its middle at `center`, scaled and rotated (in degrees) |
| `drawTexturePart(texture, source, destination, origin = {}, rotation = 0, tint = White)` | Advanced: draws the `source` part of the picture, stretched into the `destination` rectangle. `origin` is the point (inside destination) it rotates around. |
| `setTextureSmooth(texture, true)` | Smooth scaling, for high-resolution art (pixel art stays crisp by default) |

**Tint** multiplies the picture's colors. `Color::White` leaves it unchanged,
`Color::Red` makes it reddish, and `Color::White.withAlpha(0.5f)` makes it half
see-through. Tinting a white picture gives you exactly the tint color, which is
a cheap way to get many colors from one picture.

---

## Sprites

A **sprite** is a picture that knows where it is and how it looks: its position,
size, rotation, which way it faces and which animation frame it shows.

```cpp
Sprite player{"assets/player.png"};

player.position = {400, 300};   // where its MIDDLE goes
player.scale = {2, 2};          // twice as big
player.rotation = 15;           // turned 15 degrees clockwise
player.flipX = true;            // mirrored, facing the other way
player.tint = Color::Red;       // reddish
player.draw();
```

| Member | What it does | Starts as |
|---|---|---|
| `position` | Where the sprite is drawn (its middle, unless you change `anchor`) | `{0, 0}` |
| `scale` | `{1, 1}` is normal size, `{2, 2}` is twice as big | `{1, 1}` |
| `rotation` | Turn, in degrees, clockwise | `0` |
| `anchor` | Which point of the picture is "the position": `{0, 0}` = top-left, `{0.5, 0.5}` = middle, `{0.5, 1}` = bottom middle (feet) | `{0.5, 0.5}` |
| `flipX`, `flipY` | Mirror left-right or upside-down | `false` |
| `tint` | Color multiplier | `Color::White` |
| `visible` | `false` hides it | `true` |

| Function | What it does |
|---|---|
| `Sprite{"assets/file.png"}` | Makes a sprite from a picture |
| `setImage("assets/other.png")` | Changes the picture |
| `draw()` | Draws it at `position` |
| `draw(somewhere)` | Draws it at another position |
| `width()`, `height()`, `size()` | Size on screen (one frame, after scaling) |
| `bounds()` | The rectangle it covers (ignores rotation) |
| `texture()` | The picture it uses |

### A sprite inside an entity

Most of the time, a sprite belongs to an entity. Give the entity a `Sprite`
member and draw it at the entity's position:

```cpp
struct Player : Entity {
    Sprite sprite{"assets/player.png"};

    void start() override {
        size = {24, 30}; // the hitbox can be smaller than the picture
    }

    void draw() override {
        sprite.draw(position);
    }
};
```

> **Hitbox tip:** make `size` a little smaller than the picture. Players feel
> cheated when they're hit by the empty corner of an enemy's picture.

### Sprite sheets and animation

A **sprite sheet** is one picture with many **frames** arranged in a grid, like a
comic strip:

```
 ┌────┬────┬────┬────┐
 │ 0  │ 1  │ 2  │ 3  │    frames are numbered left to right,
 ├────┼────┼────┼────┤    top to bottom, starting at 0
 │ 4  │ 5  │ 6  │ 7  │
 └────┴────┴────┴────┘
     4 columns, 2 rows
```

Tell the sprite how the sheet is cut up, add some animations and play one:

```cpp
sprite.setSpriteSheet(4, 2);                  // 4 columns, 2 rows = 8 frames
sprite.addAnimation("idle", 0, 1, 2);         // frames 0 to 1, 2 frames per second
sprite.addAnimation("walk", 2, 7, 10);        // frames 2 to 7, 10 frames per second
sprite.addAnimation("jump", 4, 7, 12, false); // play once, don't loop
sprite.addAnimation("blink", {0, 1, 0, 0}, 4);  // or list the frames yourself

sprite.play("walk");

// and every frame, in update():
sprite.update(dt);  // moves the animation forward
```

| Function | What it does |
|---|---|
| `setSpriteSheet(columns, rows = 1)` | Cuts the picture into a grid of equally sized frames |
| `addAnimation(name, first, last, fps, loop = true)` | An animation using frames `first` to `last` |
| `addAnimation(name, {frames...}, fps, loop = true)` | An animation using a list of frames |
| `play(name)` | Starts an animation. If it's already playing, it **keeps going** (doesn't restart). |
| `restart(name)` | Starts an animation from its first frame, even if it was already playing |
| `stop()` | Freezes on the current frame |
| `update(dt)` | Moves the animation forward. **Call this every frame!** |
| `isPlaying()` | Is an animation running? |
| `animation()` | The name of the current animation (`""` if none) |
| `isAnimationFinished()` | True once a non-looping animation reached its last frame |
| `setFrame(n)` | Shows one frame (and stops any animation) |
| `frame()`, `frameCount()` | The current frame number, and how many frames there are |

Because `play` doesn't restart an animation that's already playing, you can
safely call it every frame:

```cpp
if (move != 0) sprite.play("walk");
else sprite.play("idle");
```

### A complete example

This uses three files in the `assets` folder: `player.png` (a sprite sheet with
4 columns and 2 rows), `coin.png` and `font.ttf`.

```cpp
#include <sprout/sprout.hpp>

using namespace sprout;

struct Player : Entity {
    Sprite sprite{"assets/player.png"};

    void start() override {
        position = {400, 250};
        size = {48, 48};

        sprite.scale = {2, 2};                 // pixel art looks best scaled up
        sprite.setSpriteSheet(4, 2);           // 4 columns, 2 rows = 8 frames
        sprite.addAnimation("idle", 0, 1, 2);  // frames 0-1, 2 frames per second
        sprite.addAnimation("walk", 2, 7, 10); // frames 2-7, 10 frames per second
    }

    void update(float dt) override {
        float move = getAxis("left", "right");
        position.x += move * 200 * dt;

        if (move != 0) sprite.play("walk");
        else sprite.play("idle");

        if (move < 0) sprite.flipX = true;  // face left
        if (move > 0) sprite.flipX = false; // face right

        sprite.update(dt); // moves the animation forward
    }

    void draw() override {
        sprite.draw(position);
    }
};

struct SpriteScene : Scene {
    Texture coin;
    float spin = 0;

    void start() override {
        background = Color{40, 44, 52};
        coin = loadTexture("assets/coin.png");
        useFont("assets/font.ttf");
        spawn<Player>();
    }

    void update(float dt) override {
        spin += 180 * dt;
    }

    void draw() override {
        drawTexture(coin, {20, 80});                                  // top-left corner at (20, 80)
        drawTextureCentered(coin, {100, 88}, 2.0f);                   // twice as big
        drawTextureCentered(coin, {160, 88}, 2.0f, spin);             // spinning
        drawTextureCentered(coin, {220, 88}, 2.0f, 0, Color::Red);    // tinted red
        drawTextureCentered(coin, {280, 88}, 2.0f, 0, Color::White.withAlpha(0.4f)); // see-through
    }

    void drawUI() override {
        drawText("Walk with the arrow keys", 10, 10, 28);
    }
};

int main() {
    Game game("Sprites", 800, 450);
    game.run<SpriteScene>();
}
```

Also have a look at the example `examples/10_sprites_and_sound`.

---

## Where to get art

- **[Kenney](https://kenney.nl/assets)**: thousands of free game pictures, sounds
  and fonts. They're "CC0", which means you can use them for anything.
- **[OpenGameArt](https://opengameart.org)** and **[itch.io](https://itch.io/game-assets/free)**:
  lots of free art. Check each one's license, because some ask you to credit the
  artist.
- **Make your own** pixel art with [Piskel](https://www.piskelapp.com) (free, in
  your browser), [LibreSprite](https://libresprite.github.io) (free) or
  [Aseprite](https://www.aseprite.org) (paid). Export sprite sheets as `.png`.

PNG is the best format for game pictures, because it supports see-through
backgrounds.

**Next:** [Sound and music](08-sound-and-music.md).
