# 13. Saving: high scores, settings and JSON

When your game closes, every variable is forgotten. To remember things for next
time (a high score, the volume, which levels are unlocked) you need to **save**
them to a file.

Sprout makes this a two-line job with `SaveFile`. And because save files are
written in **JSON**, a simple text format, you can use the same tools to load
your own data, like level layouts and enemy stats.

---

## `SaveFile`

```cpp
struct PlayScene : Scene {
    SaveFile save{"savegame.json"};  // opens the file (if it exists yet)

    void start() override {
        int best = save.get("highscore", 0);  // 0 if there's no high score yet
        // ...
    }

    void gameOver(int score) {
        if (score > save.get("highscore", 0)) {
            save.set("highscore", score);     // written to the file straight away
        }
    }
};
```

That's it. No "load" or "save" buttons needed:

- Making a `SaveFile` reads the file if it's there. If it isn't (the very first
  time someone plays), you simply start with an empty one.
- `get(name, default)` reads a value. If there isn't one with that name yet,
  you get the **default** you gave it. That's why it always works, even on the
  first run.
- `set(name, value)` stores a value **and writes the file immediately**.

The **type of the default decides the type you get back**:

```cpp
int best = save.get("highscore", 0);                     // a whole number
float volume = save.get("volume", 0.8f);                 // a decimal number
bool fullscreen = save.get("fullscreen", false);         // true or false
std::string name = save.get("playerName", "nobody");     // text
std::vector<int> levels = save.get("unlocked", std::vector<int>{1}); // a list
```

If a value exists but has the wrong type (for example text where you expected
a number), you get the default back instead of a crash. Whole numbers and
decimals count as the same kind of thing (a stored `3` read with
`save.get("lives", 0.0f)` gives `3.0`), but `true`/`false` is never turned into
a number or the other way round.

### Everything a `SaveFile` can do

| Code | What it does |
|---|---|
| `SaveFile save{"name.json"};` | Opens (or starts) a save file. `SaveFile save;` uses `"save.json"`. |
| `save.get(name, default)` | Reads a value, or gives `default` if it's missing or the wrong type |
| `save.set(name, value)` | Stores a value and writes the file |
| `save.has(name)` | Is there a value with this name? |
| `save.remove(name)` | Deletes one value (and writes the file) |
| `save.clear()` | Deletes everything (and writes the file). A "reset progress" button! |
| `save.write()` | Writes the file. `set`, `remove` and `clear` already do this for you. |
| `save.reload()` | Reads the file again |
| `save.data()` | The whole save as `Json`, for advanced use (call `write()` after changing it) |
| `save.path()` | Where the file really is (the full path) |

### Where to create it

The easiest place is as a **member of your scene**, with **curly brackets**:

```cpp
struct PlayScene : Scene {
    SaveFile save{"savegame.json"};   // good
    // SaveFile save("savegame.json"); // error: round brackets don't work here
};
```

Each time the scene starts, its `SaveFile` reads the latest version of the
file. If several scenes need the same save, you can also make **one** `SaveFile`
as a global variable (at the top of your file, outside of any struct) and use it
from every scene:

```cpp
SaveFile save{"savegame.json"}; // shared by all scenes
```

Either way, `"savegame.json"` always means a file in your game's own folder.
Just don't have two different `SaveFile`s for the **same** file alive at the
same time: each one keeps its own copy of the data, so they would overwrite
each other's changes.

### Where is the file?

The save file is written **next to your game's program**, for example
`build/bin/my_game/savegame.json`. Open it in any text editor to see what's
inside:

```json
{
    "highscore": 1200,
    "playerName": "Sam",
    "volume": 0.8
}
```

To start over while testing, delete that file (or call `save.clear()`).
Rebuilding your game doesn't delete it.

> **Your saves are safe.** Sprout writes a save to a temporary file first and
> only then swaps it in. If the game crashes in the middle of saving, or saving
> fails, the old save is still there. And if a save file gets broken (say,
> someone edited it by hand and made a mistake), Sprout prints a warning and
> starts with an empty save instead of crashing.

---

## A complete example: a clicker with a high score

Click the circle as many times as you can in 10 seconds. Your best score is
remembered, even after you close the game. Press Delete to forget it.

```cpp
#include <sprout/sprout.hpp>

using namespace sprout;

struct ClickerScene : Scene {
    SaveFile save{"clicker.json"}; // loads the file if it's already there
    int clicks = 0;
    int best = 0;
    Timer clock{10.0f};

    void start() override {
        background = Color::DarkBlue;
        best = save.get("best", 0); // 0 if there's no best score yet
    }

    void update(float) override {
        if (clock.isDone()) {
            if (isKeyPressed(Key::Enter)) restartScene();
            return;
        }

        if (isMousePressed() && mousePosition().distanceTo({400, 240}) < 80) {
            clicks += 1;
            particles.burst(mousePosition(), Color::Gold, 10);

            if (clicks > best) {
                best = clicks;
                save.set("best", best); // written to the file straight away
            }
        }

        if (isKeyPressed(Key::Delete)) {
            save.clear(); // forget everything
            best = 0;
        }
    }

    void draw() override {
        drawCircle({400, 240}, 80, clock.isDone() ? Color::Gray : Color::Orange);
    }

    void drawUI() override {
        drawText("Clicks: " + std::to_string(clicks), 10, 10, 30);
        drawText("Best: " + std::to_string(best), 10, 45, 30, Color::Gold);
        drawText("Time: " + std::to_string(int(clock.timeLeft())), 650, 10, 30);
        drawTextCentered("Click the circle as fast as you can!", {400, 380}, 24);
        if (clock.isDone()) drawTextCentered("Time's up! Enter: try again", {400, 420}, 24, Color::Yellow);
    }
};

int main() {
    Game game("Clicker", 800, 450);
    game.run<ClickerScene>();
}
```

> **Try it!** Add the high score to the tutorial game from
> [Your first game](02-your-first-game.md): give `PlayScene` a `SaveFile`, and
> when the game is over, `set` the score if it beats the old best. Show the best
> score in the game-over message.

### Saving settings

Settings work the same way. Load them when the game starts and save them when
they change:

```cpp
struct SettingsMenu : Scene {
    SaveFile settings{"settings.json"};
    float volume = 1.0f;

    void start() override {
        volume = settings.get("volume", 1.0f);
    }

    void drawUI() override {
        if (slider("Volume", {310, 90, 200, 24}, volume, 0.0f, 1.0f)) {
            setMasterVolume(volume);
            settings.set("volume", volume);
        }
    }
};
```

Remember to also **apply** saved settings when the game starts (for example,
call `setMasterVolume(settings.get("volume", 1.0f));` in your first scene's
`start()`), not only in the settings menu.

---

## JSON: data in a text file

**JSON** (say "jay-son") is a very popular way to write data as text. Lots of
programs and websites use it. It looks like this:

```json
{
    "name": "The Big Cave",
    "difficulty": 3,
    "hasBoss": true,
    "music": "assets/cave.ogg",
    "coins": [10, 25, 40],
    "player": { "x": 100, "y": 300 }
}
```

The building blocks:

| JSON | What it is | Example |
|---|---|---|
| number | a whole or decimal number | `3`, `-12`, `0.5` |
| text (a "string") | in double quotes | `"The Big Cave"` |
| `true` / `false` | yes or no | `true` |
| list (an "array") | values in `[ ]`, separated by commas | `[10, 25, 40]` |
| object | `"name": value` pairs in `{ }`, separated by commas | `{ "x": 100, "y": 300 }` |
| `null` | nothing | `null` |

Lists and objects can contain each other, as deep as you like. Watch out for
the commas: there's one **between** items, but not after the last one.

Sprout uses the [nlohmann/json](https://github.com/nlohmann/json) library for
JSON, and calls its type `Json`.

### Reading JSON files

`loadJson(path)` reads a JSON file. Put your data files in the `assets` folder,
just like pictures:

```cpp
Json level = loadJson("assets/level1.json");

std::string name = level["name"];       // "The Big Cave"
int difficulty = level["difficulty"];   // 3
float x = level["player"]["x"];          // 100
```

`level["name"]` looks up a value by its name, and `["player"]["x"]` goes one
level deeper. If the file is missing or has a mistake in it, Sprout prints a
warning explaining what went wrong, and you get an empty `Json` (`null`).

> **Careful with missing values.** `int hp = data["health"];` stops the game
> with an error if there's no `"health"` (or it isn't a number). The safe way is
> `value(name, default)`, which works just like `SaveFile::get`:
>
> ```cpp
> int hp = data.value("health", 10);  // 10 if "health" is missing
> ```
>
> `value` only works on an object (the `{ }` kind). If the whole file might be
> missing, check first: `if (data.is_object()) { ... }`.

Going through a list:

```cpp
for (int amount : level["coins"]) {
    // amount is 10, then 25, then 40
}
```

Some other handy functions: `data.contains("name")` (is it there?),
`data.size()` (how many items in a list or object), `data.is_null()` (is it
empty?), and `data["x"].get<float>()` (get it as a particular type, handy when
C++ can't work out the type by itself).

### Example: enemy stats in a file

Keeping numbers like health and speed in a file makes them easy to tweak
without changing code. `assets/enemies.json`:

```json
{
    "slime": { "health": 3, "speed": 40 },
    "bat":   { "health": 1, "speed": 120 },
    "ghost": { "health": 5, "speed": 60 }
}
```

```cpp
struct Enemy : Entity {
    int health = 1;
    float speed = 50;

    Enemy(std::string type, Vec2 where) {
        position = where;

        Json stats = loadJson("assets/enemies.json");
        health = stats[type].value("health", 1); // 1 if it's missing
        speed = stats[type].value("speed", 50.0f);
    }
};

// in the scene:
spawn<Enemy>("bat", Vec2{300, 100});
```

### Example: a whole level in a file

`assets/level1.json`:

```json
{
    "name": "The Big Cave",
    "tiles": [
        "##########",
        "#........#",
        "#..P.....#",
        "##########"
    ],
    "enemies": [
        { "type": "slime", "x": 200, "y": 300 },
        { "type": "bat",   "x": 500, "y": 120 }
    ]
}
```

```cpp
struct Level : Scene {
    TileMap map;
    std::string name;

    void start() override {
        Json level = loadJson("assets/level1.json");

        name = level.value("name", "Unnamed level");
        map = TileMap{32, level["tiles"].get<std::vector<std::string>>()};

        for (Json& enemy : level["enemies"]) {
            std::string type = enemy["type"];
            float x = enemy["x"];
            float y = enemy["y"];
            spawn<Enemy>(type, Vec2{x, y});
        }
    }
};
```

`level["tiles"].get<std::vector<std::string>>()` turns the JSON list of rows
into the list of strings a `TileMap` wants.

> **Why `float x = enemy["x"];` and not `Vec2{enemy["x"], enemy["y"]}`?** `Vec2`
> only accepts real numbers, and a `Json` value isn't one until you say which
> kind of number you want. Storing it in a `float` first (or writing
> `enemy["x"].get<float>()`) does that.

### Writing JSON files

You can build `Json` yourself and write it with `saveJson(path, data)`:

```cpp
Json data;
data["name"] = "Sam";
data["score"] = 1200;
data["levels"] = {1, 2, 3};      // a list
data["player"]["x"] = 10.5;     // an object inside an object

saveJson("mydata.json", data);  // gives false if it couldn't be written
```

The file is nicely indented, so it's easy to read. Like save files, it's written
next to your game's program.

> **Don't save into `assets/`.** The `assets` folder next to your program is a
> **copy** of the one next to your `main.cpp`, and it's copied again every time
> you build, which would overwrite your changes. Keep your hand-made data in
> `games/my_game/assets/`, and save things the game writes (like saves) outside
> of `assets`.

**Next:** [Math and random numbers](14-math-and-random.md).
