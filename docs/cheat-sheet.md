# Sprout cheat sheet

Everything in Sprout on one page, grouped by header file. One line gets you all
of it:

```cpp
#include <sprout/sprout.hpp>
using namespace sprout;
```

`= value` means "you can leave this out, and then it's this value".
Each header in `engine/include/sprout/` also starts with a comment explaining
how to use it.

**Jump to:** [Game](#game-gamehpp) · [Scene](#scene-scenehpp) ·
[Entity](#entity-entityhpp) · [Input](#input-inputhpp) ·
[Drawing](#drawing-graphicshpp) · [Sprite](#sprite-spritehpp) ·
[Camera](#camera-camerahpp) · [TileMap](#tilemap-tilemaphpp) ·
[Physics](#physics-physicshpp) · [Particles](#particles-particleshpp) ·
[Audio](#audio-audiohpp) · [Timers](#timers-timerhpp) · [UI](#ui-uihpp) ·
[Saving](#saving-savehpp) · [Random](#random-randomhpp) ·
[Math](#math-mathhpp) · [Color](#color-colorhpp) ·
[raylib interop](#raylib-interop-raylib_interophpp-advanced)

---

## Game (`game.hpp`)

| Code | What it does |
|---|---|
| `Game game(title, width = 800, height = 450);` | Opens the window (only one Game at a time) |
| `game.run<FirstScene>(values...);` | Runs the game until it's closed. Values go to the scene's constructor. |
| `changeScene<Scene>(values...)` | Switches scene at the end of this frame |
| `restartScene()` | Starts the current scene again, with the same values |
| `currentScene()` | The running scene (`Scene&`) |
| `quitGame()` | Closes the game at the end of this frame |
| `screenWidth()`, `screenHeight()` | Window size in pixels (`int`) |
| `screenSize()`, `screenCenter()` | Window size, and its middle (`Vec2`) |
| `setWindowTitle(title)` | Changes the window title |
| `setResizable(bool)` | Lets the player resize the window |
| `setFullscreen(bool)`, `isFullscreen()`, `toggleFullscreen()` | Fullscreen (borderless) |
| `showCursor()`, `hideCursor()` | The mouse pointer |
| `setExitKey(Key)` | Which key quits (`Key::Escape` by default, `Key::None` = none) |
| `takeScreenshot(path = "screenshot.png")` | Saves a picture of the screen (F12 also works) |
| `deltaTime()` | Seconds since the last frame (same as `dt`) |
| `timeSinceStart()` | Seconds since the game started |
| `fps()` | Frames per second right now |
| `setTargetFPS(fps)` | Frames per second to aim for (60 by default) |
| `SPROUT_VERSION` | The Sprout version, as text (`"0.1.0"`) |

## Scene (`scene.hpp`)

```cpp
struct MyScene : Scene {
    void start() override {}            // once, at the beginning
    void update(float dt) override {}   // every frame
    void draw() override {}             // every frame: the world, through the camera
    void drawUI() override {}           // every frame: on top, no camera
};
```

| Member | What it is | Default |
|---|---|---|
| `background` | Color the screen is wiped with | `Color::Black` |
| `camera` | The scene's [Camera](#camera-camerahpp) | looks at the window's middle |
| `particles` | The scene's [ParticleSystem](#particles-particleshpp) | |
| `paused` | `true` freezes entities, particles, timers and shake (your `update` still runs) | `false` |

| Code | What it does |
|---|---|
| `spawn<T>(values...)` | Makes an entity, runs its `start()`, gives back `T&` |
| `findAll<T>()` | Every `T` (a `std::vector<T*>`). Leave out `<T>` for every entity. |
| `findFirst<T>()` | The first `T`, or `nullptr` |
| `findClosest<T>(point)` | The `T` closest to a point, or `nullptr` |
| `count<T>()` | How many `T`s there are |
| `destroyAll<T>()` | Destroys every `T` (every entity without `<T>`) |
| `after(seconds, [this] { ... })` | Runs code once, later. Gives back an id. |
| `every(seconds, [this] { ... })` | Runs code again and again. Gives back an id. |
| `cancelTimer(id)`, `cancelAllTimers()` | Stops `after`/`every` timers |
| `addSolid(rect)`, `addSolid(map)` | Makes a rectangle or a tile map's solid tiles block `moveAndSlide` |
| `clearSolids()` | Removes all solids |
| `solidRects()`, `solidMaps()` | The solids added so far |

## Entity (`entity.hpp`)

```cpp
struct Coin : Entity {
    Coin(Vec2 where) { position = where; size = {16, 16}; }
    void start() override {}            // right after spawning (scene() works here)
    void update(float dt) override {}   // every frame
    void draw() override { drawCircle(position, 8, Color::Gold); }
};
```

| Member | What it is | Default |
|---|---|---|
| `position` | The **middle** of the entity | `{0, 0}` |
| `size` | Hitbox width and height | `{32, 32}` |
| `velocity` | Pixels per second (used by `moveAndSlide`) | `{0, 0}` |
| `layer` | Higher = drawn on top | `0` |
| `visible` | `false` = not drawn (still updates) | `true` |

| Code | What it does |
|---|---|
| `bounds()` | The hitbox as a `Rect` |
| `overlaps(entity)`, `overlaps(rect)` | Do the hitboxes touch? |
| `findOverlapping<T>()` | The first `T` touching me, or `nullptr` |
| `findAllOverlapping<T>()` | Every `T` touching me |
| `moveAndSlide(dt)` | Moves by `velocity * dt`, stopping at solids. Gives back a `MoveResult`. |
| `isOnFloor()`, `isOnWall()`, `isOnCeiling()` | What the last `moveAndSlide` touched |
| `destroy()`, `isDestroyed()` | Remove at the end of the frame / was it? |
| `scene()` | The scene I'm in (not in the constructor) |

## Input (`input.hpp`)

| Code | What it does |
|---|---|
| `isKeyDown(Key::A)` | Held right now |
| `isKeyPressed(Key::A)` | Pressed this frame |
| `isKeyReleased(Key::A)` | Let go this frame |
| `typedText()` | Letters typed this frame (`std::string`) |
| `isMouseDown(button = MouseButton::Left)` | Mouse button held (`Left`, `Right`, `Middle`) |
| `isMousePressed(button = Left)`, `isMouseReleased(button = Left)` | Clicked / let go this frame |
| `mousePosition()` | Mouse on the screen |
| `mouseWorldPosition()` | Mouse in the world (through the camera) |
| `mouseWheel()` | Wheel movement this frame (up = positive) |
| `isGamepadConnected(pad = 0)` | Is that gamepad plugged in? |
| `isGamepadDown(GamepadButton::A, pad = 0)` | Gamepad button held (`Pressed`, `Released` too) |
| `gamepadAxis(GamepadAxis::LeftX, pad = 0)` | Stick -1 to 1 (small wobbles ignored), triggers up to 1 |
| `bindAction("name", Key::X)` | Adds a key to an action (duplicates are ignored) |
| `bindAction("name", MouseButton::Left)` | ...a mouse button |
| `bindAction("name", GamepadButton::A, pad = 0)` | ...a gamepad button |
| `bindAction("name", GamepadAxis::LeftX, -1 or 1, pad = 0)` | ...a stick direction |
| `clearAction("name")` | Removes every key/button from an action |
| `isActionDown("name")`, `isActionPressed(...)`, `isActionReleased(...)` | Like the key functions |
| `getAxis("left", "right")` | -1, 0 or +1 |
| `getVector("left", "right", "up", "down")` | Direction with length 0 or 1 |

**Ready-made actions:** `"left"` (Left, A), `"right"` (Right, D), `"up"` (Up,
W) and `"down"` (Down, S), which also work with the D-pad and left stick, plus
`"jump"` (Space, gamepad A) and `"pause"` (P, gamepad Start).

**Keys:** `Key::A`...`Z`, `Zero`...`Nine`, `Space`, `Enter`, `Escape`, `Tab`,
`Backspace`, `Delete`, `Insert`, `Up`, `Down`, `Left`, `Right`, `Home`, `End`,
`PageUp`, `PageDown`, `LeftShift`, `RightShift`, `LeftControl`,
`RightControl`, `LeftAlt`, `RightAlt`, `LeftSuper`, `RightSuper`, `Menu`,
`F1`...`F12`, `Keypad0`...`Keypad9`, `KeypadDecimal`, `KeypadDivide`,
`KeypadMultiply`, `KeypadSubtract`, `KeypadAdd`, `KeypadEnter`, `KeypadEqual`,
`Apostrophe`, `Comma`, `Minus`, `Period`, `Slash`, `Semicolon`, `Equal`,
`LeftBracket`, `Backslash`, `RightBracket`, `Grave`, `CapsLock`,
`ScrollLock`, `NumLock`, `PrintScreen`, `Pause`, `None`.

**Gamepad buttons:** `A`, `B`, `X`, `Y`, `DPadUp`, `DPadDown`, `DPadLeft`,
`DPadRight`, `LeftBumper`, `RightBumper`, `LeftTrigger`, `RightTrigger`,
`Select`, `Start`, `Home`, `LeftStick`, `RightStick` (`Unknown`).
**Axes:** `LeftX`, `LeftY`, `RightX`, `RightY`, `LeftTrigger`, `RightTrigger`.

## Drawing (`graphics.hpp`)

| Code | What it draws |
|---|---|
| `drawRect(x, y, w, h, color)`, `drawRect(rect, color)` | Filled rectangle (top-left corner) |
| `drawRectOutline(rect, color, thickness = 2)` | Rectangle border |
| `drawRoundedRect(rect, cornerRadius, color)` | Rounded rectangle |
| `drawCircle(center, radius, color)`, `drawCircle(x, y, radius, color)` | Filled circle (middle) |
| `drawCircleOutline(center, radius, color, thickness = 2)` | Ring |
| `drawLine(from, to, color, thickness = 2)` | Line |
| `drawTriangle(a, b, c, color)` | Triangle (any corner order) |
| `drawPolygon(center, sides, radius, color, rotation = 0)` | Regular shape |
| `drawText(text, x, y, size, color = White)`, `drawText(text, position, size, color = White)` | Text (top-left corner) |
| `drawTextCentered(text, center, size, color = White)` | Text centered on a point |
| `measureText(text, size)` | Width of text in pixels |
| `drawFPS(x = 10, y = 10)` | Frames-per-second counter |
| `useFont(path, quality = 64)`, `useDefaultFont()` | Font for all text (.ttf/.otf) |
| `loadTexture(path)` | Loads a picture (cached; checkerboard if missing) |
| `drawTexture(texture, position, tint = White)` | Picture (top-left corner) |
| `drawTextureCentered(texture, center, scale = 1, rotation = 0, tint = White)` | Picture centered, scaled, rotated |
| `drawTexturePart(texture, source, destination, origin = {}, rotation = 0, tint = White)` | Part of a picture into a rectangle |
| `setTextureSmooth(texture, bool)` | Smooth scaling (off = crisp pixels) |
| `Texture`: `width`, `height`, `size()`, `isValid()` | A loaded picture (cheap to copy) |

## Sprite (`sprite.hpp`)

| Member | What it is | Default |
|---|---|---|
| `position` | Where the anchor point goes | `{0, 0}` |
| `scale` | Size multiplier | `{1, 1}` |
| `rotation` | Degrees, clockwise | `0` |
| `anchor` | `{0,0}` top-left ... `{0.5,0.5}` middle ... `{1,1}` bottom-right | `{0.5, 0.5}` |
| `flipX`, `flipY` | Mirror | `false` |
| `tint` | Color multiplier | `Color::White` |
| `visible` | Drawn or not | `true` |

| Code | What it does |
|---|---|
| `Sprite s{"assets/p.png"};`, `Sprite s{texture};`, `Sprite s;` | Make a sprite |
| `setImage(path)`, `setImage(texture)`, `texture()` | The picture |
| `setSpriteSheet(columns, rows = 1)` | Cut the picture into frames |
| `setFrame(n)`, `frame()`, `frameCount()` | Show one frame (stops animation) / which / how many |
| `addAnimation(name, first, last, fps, loop = true)` | Animation from a range of frames |
| `addAnimation(name, {0, 1, 2, 1}, fps, loop = true)` | Animation from a list of frames |
| `play(name)` | Start it (keeps going if it's already the current one) |
| `restart(name)` | Start it from the first frame |
| `stop()`, `isPlaying()`, `animation()`, `isAnimationFinished()` | Control and check |
| `update(dt)` | Moves the animation on (call every frame) |
| `draw()`, `draw(at)` | Draw at `position`, or somewhere else |
| `width()`, `height()`, `size()`, `bounds()` | Size on screen (one frame, scaled) |

## Camera (`camera.hpp`)

| Code | What it does |
|---|---|
| `camera.position` | World point in the middle of the screen |
| `camera.zoom` | 1 = normal, 2 = bigger, 0.5 = see more (default 1) |
| `camera.rotation` | Tilt in degrees (default 0) |
| `camera.follow(target, smoothness = 0)` | Look at a point (3-10 = smooth) |
| `camera.shake(strength = 8, duration = 0.25)` | Shake the screen |
| `camera.setLimits(rect)`, `camera.clearLimits()` | Never show outside an area |
| `camera.screenToWorld(point)`, `camera.worldToScreen(point)` | Convert coordinates |
| `camera.visibleArea()` | What's on screen, as a `Rect` |
| `camera.reset()` | Middle of the window, no zoom, no limits |

## TileMap (`tilemap.hpp`)

| Code | What it does |
|---|---|
| `TileMap map{32, {"####", "#P.#", "####"}};` | Map from letters, 32-pixel tiles |
| `TileMap::fromFile("assets/level.txt", 32)` | Map from a text file |
| `map.position` | World position of the top-left corner |
| `tileSize()`, `columns()`, `rows()`, `width()`, `height()`, `bounds()` | Size |
| `getTile(column, row)`, `setTile(column, row, 'X')` | Read/change a tile (`' '` outside) |
| `tileAt(point)`, `setTileAt(point, 'X')` | Tile under a world point |
| `coordAt(point)` | `TileCoord` with `.column` and `.row` |
| `tileCenter(column, row)`, `tileRect(column, row)` | Where a tile is |
| `findTiles('P')` | Middles of all `P` tiles (`std::vector<Vec2>`) |
| `replaceTiles('P', '.')`, `countTiles('C')` | Change all / count |
| `setSolid("#=")`, `addSolid('X')`, `isSolid('#')`, `isSolidAt(point)` | Solid letters |
| `solidRectsIn(area)` | Rects of solid tiles in an area |
| `setColor('#', color)` | Draw a letter as a colored square |
| `setImage('#', path)`, `setImage('#', path, partRect)` | Draw a letter as a picture / part of a tile set |
| `draw()` | Draw the map (only the visible part) |

## Physics (`physics.hpp`)

| Code | What it does |
|---|---|
| `MoveResult`: `hitLeft`, `hitRight`, `hitTop`, `hitBottom`, `hitWall()`, `hitAnything()` | What a move bumped into |
| `moveAndCollide(rect, motion, std::vector<Rect> solids)` | Moves a `Rect`, stopping at solids |
| `moveAndCollide(rect, motion, tileMap)` | Same, against a tile map's solid tiles |

## Particles (`particles.hpp`)

| Code | What it does | Default |
|---|---|---|
| `particles.burst(pos, color, count = 20, speed = 150, lifetime = 0.6, size = 4)` | Explosion in every direction | |
| `particles.spray(pos, color, direction, spread, count = 5, speed = 150, lifetime = 0.5, size = 3)` | A fan in one direction (degrees) | |
| `particles.add(particle)` | One `Particle` (`position`, `velocity`, `color`, `size`, `life`, `maxLife`) | |
| `particles.gravity` | Pull downwards | `0` |
| `particles.drag` | Slowing down | `1` |
| `particles.shrink`, `particles.fade` | Shrink / fade as they die | `true` |
| `particles.clear()`, `particles.count()` | Remove all / how many | |

## Audio (`audio.hpp`)

| Code | What it does |
|---|---|
| `playSound(path, volume = 1, pitch = 1)` | Sound effect (can overlap itself) |
| `stopSound(path)`, `isSoundPlaying(path)`, `preloadSound(path)` | Control sound effects |
| `playMusic(path, volume = 1, loop = true)` | Background music (one song at a time; same song = keeps playing) |
| `stopMusic()`, `pauseMusic()`, `resumeMusic()`, `isMusicPlaying()` | Control music |
| `setMusicVolume(volume)` | Volume of the current song |
| `setMasterVolume(volume)`, `masterVolume()` | Volume of everything (0 to 1) |

Formats: `.wav`, `.ogg`, `.mp3`, `.flac`, `.qoa`. No sound card = silently nothing.

## Timers (`timer.hpp`)

| Code | What it does |
|---|---|
| `Timer t{seconds};` | Done after that many seconds (starts now) |
| `t.isDone()`, `t.restart()`, `t.restart(seconds)` | Check / start again |
| `t.timeLeft()`, `t.elapsed()`, `t.progress()`, `t.duration()` | Seconds left / passed, 0-1 progress, length |
| `Cooldown c{seconds};` | Usable once every that many seconds (starts ready) |
| `c.use()` | If ready: start the cooldown and give `true` |
| `c.isReady()`, `c.reset()`, `c.timeLeft()`, `c.duration()`, `c.setDuration(s)` | Check and control |

Both stop counting while the scene is paused. See also `after` and `every` in
[Scene](#scene-scenehpp).

## UI (`ui.hpp`)

Call these in `drawUI()`. `area` is a `Rect` `{x, y, width, height}`.

| Code | What it does |
|---|---|
| `button(text, area)` | `true` on the frame it's clicked |
| `checkbox(text, area, boolVariable)` | Toggles the bool; `true` if it changed (text on the right) |
| `slider(text, area, floatVariable, min, max)` | Changes the float; `true` if it changed (text on the left) |
| `textBox(area, stringVariable, maxLength = 32)` | Typing box; `true` if the text changed |
| `progressBar(area, value, max = 1)` | Bar `value / max` full |
| `label(text, area)` | Text in the UI style |
| `panel(title, area)` | Box with a title bar |
| `setUITextSize(size)` | UI text size (default 20; kept when the theme changes) |
| `setUITheme(UITheme::Dark)` | `Default`, `Dark`, `Candy`, `Cyber`, `Jungle`, `Sunny`, `Terminal` |

## Saving (`save.hpp`)

| Code | What it does |
|---|---|
| `SaveFile save{"save.json"};` | Opens a save in the game's folder (empty if new) |
| `save.get("name", default)` | Reads a value, or `default` if missing/wrong type |
| `save.set("name", value)` | Stores a value and writes the file |
| `save.has(name)`, `save.remove(name)`, `save.clear()` | Check / delete one / delete all |
| `save.write()`, `save.reload()` | Write / read again (rarely needed) |
| `save.data()`, `save.path()` | The whole `Json` / the file's full path |
| `Json` | A JSON value (from nlohmann/json) |
| `loadJson(path)` | Reads a JSON file (`null` if missing or broken) |
| `saveJson(path, json)` | Writes a JSON file; `false` if it failed |
| `json["key"]`, `json.value("key", default)`, `json.contains("key")`, `json.get<float>()` | Reading JSON |

## Random (`random.hpp`)

| Code | What you get |
|---|---|
| `randomInt(min, max)` | Whole number, both ends included |
| `randomFloat(min, max)` | Decimal number |
| `chance(0.25f)` | `true` 25% of the time |
| `randomDirection()` | Length-1 `Vec2`, any angle |
| `randomPointIn(rect)` | Point inside a rectangle |
| `randomColor()` | A bright color |
| `randomChoice(vector)` | A random item (list must not be empty) |
| `setRandomSeed(number)` | Same "random" numbers every run |

## Math (`math.hpp`)

**`Vec2{x, y}`**: `+`, `-`, `*` and `/` (with a number or another `Vec2`),
`-v`, `+=`, `-=`, `*=`, `/=`, `==`, `2 * v`.

| Code | What you get |
|---|---|
| `v.length()`, `v.lengthSquared()` | Length (and length squared) |
| `v.normalized()` | Same direction, length 1 |
| `a.distanceTo(b)`, `a.directionTo(b)` | Distance, and length-1 direction from a to b |
| `v.angle()`, `a.angleTo(b)` | Angle in degrees (0 = right, 90 = down) |
| `v.rotated(degrees)` | Turned clockwise |
| `a.dot(b)` | Dot product |
| `Vec2::fromAngle(degrees, length = 1)` | `Vec2` from an angle |

**`Rect{x, y, width, height}`** (x, y = top-left), `Rect{topLeft, size}`,
`Rect::fromCenter(center, size)`.

| Code | What you get |
|---|---|
| `r.left()`, `r.right()`, `r.top()`, `r.bottom()` | Edges |
| `r.position()`, `r.size()`, `r.center()` | Corner, size, middle |
| `r.setCenter(point)` | Moves it |
| `r.overlaps(other)`, `r.contains(point)` | Collision checks |
| `r.grown(amount)` | Bigger (or smaller, if negative) copy |

| Code | What you get |
|---|---|
| `clamp(value, min, max)`, `clamp(point, rect)` | Kept within limits |
| `lerp(a, b, t)` | Blend (numbers or `Vec2`s; t from 0 to 1) |
| `moveToward(current, target, maxStep)` | One step closer (numbers or `Vec2`s) |
| `wrap(value, min, max)` | Wrap around: `wrap(810, 0, 800)` is 10 |
| `toRadians(degrees)`, `toDegrees(radians)`, `Pi` | Angle units |

## Color (`color.hpp`)

| Code | What you get |
|---|---|
| `Color{r, g, b}`, `Color{r, g, b, a}` | 0-255 each; `a` = alpha (0 invisible, 255 solid) |
| `Color::fromHex(0xFF8800)` | Web-style color |
| `Color::fromHSV(hue, saturation = 1, value = 1)` | Rainbow position 0-360 |
| `c.withAlpha(0.5f)` | See-through copy (0 to 1) |
| `c.lighter(amount = 0.25)`, `c.darker(amount = 0.25)` | Lighter / darker |
| `Color::mix(a, b, t)` | Blend two colors |
| `c.r`, `c.g`, `c.b`, `c.a` | The parts |

Ready-made: `White`, `Black`, `Blank`, `Transparent`, `LightGray`, `Gray`,
`DarkGray`, `Red`, `Maroon`, `Pink`, `Orange`, `Gold`, `Yellow`, `Green`,
`Lime`, `DarkGreen`, `SkyBlue`, `Blue`, `DarkBlue`, `Cyan`, `Purple`, `Violet`,
`DarkPurple`, `Magenta`, `Beige`, `Brown`, `DarkBrown`, `OffWhite` (all
`Color::Name`).

## raylib interop (`raylib_interop.hpp`, advanced)

Not included by `sprout.hpp`. Include `<sprout/raylib_interop.hpp>` yourself to
use [raylib-cpp](https://github.com/RobLoach/raylib-cpp) alongside Sprout. See
[Using raylib-cpp directly](using-sprout-in-your-own-project.md#going-further-using-raylib-cpp-directly).

| Code | What you get |
|---|---|
| `toRaylib(Vec2)`, `toRaylib(Rect)`, `toRaylib(Color)` | `raylib::Vector2`, `raylib::Rectangle`, `raylib::Color` |
| `toRaylib(Texture)` | `raylib::TextureUnmanaged` (Sprout still owns it) |
| `toRaylib(Camera)` | `raylib::Camera2D` (with shake) |
| `fromRaylib(::Vector2 / ::Rectangle / ::Color / ::Texture)` | `Vec2` / `Rect` / `Color` / `Texture` |

In such files, write `sprout::Color`, `sprout::Texture`, `sprout::Camera`,
`sprout::MouseButton`, `sprout::GamepadButton` and `sprout::GamepadAxis` in
full (raylib has types with the same names).
