# Troubleshooting

Something not working? You're in good company: every game developer spends a
lot of time fixing problems. This page lists the most common ones and how to fix
them.

**First, two golden rules:**

1. **Read the first error.** When the compiler finds a mistake, it often prints
   a whole screen of messages. Scroll **up** to the **first** line that says
   `error`. That's usually the real problem, and the rest are follow-on
   confusion. It tells you the file, the line number and (roughly) what's wrong:
   `main.cpp:42:10: error: expected ';' ...` means "line 42, column 10".
2. **Check the terminal while your game runs.** Sprout prints friendly warnings
   there, starting with `[Sprout] Warning:`, such as when a picture can't be
   found.

Jump to: [Installing and building](#installing-and-building) ·
[Compile errors](#compile-errors) · [Link errors](#link-errors) ·
[When the game runs](#when-the-game-runs) · [The Sprout Editor](#the-sprout-editor)

Most of this page is about the code-only Sprout Engine. Problems with the
visual [Sprout Editor](../editor/README.md) are in their own section at the end.

---

## Installing and building

### "cmake is not recognized" / "cmake: command not found"

CMake isn't installed, or your terminal can't find it.

- **Windows:** use the **Developer PowerShell for VS** (search for it in the
  Start menu). The CMake that comes with Visual Studio only works there. Or
  install CMake from <https://cmake.org/download/> and choose "Add CMake to the
  PATH".
- **macOS:** `brew install cmake`.
- **Linux:** `sudo apt install cmake` (or your system's equivalent).

Close and reopen the terminal after installing.

### "CMake 3.25 or higher is required"

Your CMake is too old for raylib 6. Check with `cmake --version`.

- **Ubuntu 22.04 and other older Linux systems:**
  `sudo snap install cmake --classic` or `pip install cmake`.
- **Windows:** update Visual Studio (Visual Studio Installer > Update), or
  install the newest CMake from cmake.org.
- **macOS:** `brew upgrade cmake`.

Then delete the `build` folder and build again.

### "git is not recognized", "could not find git", or "Failed to clone repository"

CMake uses Git to download raylib and the other libraries the first time.

- Install Git (see [Installing](01-installing.md)) and open a new terminal.
- Check your internet connection. School and work networks sometimes block
  GitHub; try another network.
- After fixing it, delete the `build` folder and build again.

### "No CMAKE_CXX_COMPILER could be found"

There's no C++ compiler.

- **Windows:** in the Visual Studio Installer, make sure the **Desktop
  development with C++** workload is installed, and use the Developer PowerShell.
- **macOS:** `xcode-select --install`.
- **Linux:** `sudo apt install build-essential` (Fedora: `sudo dnf install gcc-c++`).

### The first build takes ages

That's normal the first time: CMake downloads raylib and friends and compiles
all of raylib. It can take several minutes. Later builds only recompile what
you changed and take seconds. Add `--parallel` to use all your processor cores:
`cmake --build build --parallel`.

### Linux: "X11/Xlib.h: No such file or directory", "Could NOT find X11", "GL/gl.h" or "alsa/asoundlib.h"

The development packages for windows, graphics or sound are missing. Install
them (Ubuntu/Debian):

```bash
sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libasound2-dev
```

See [Installing](01-installing.md#linux) for Fedora and Arch. Then delete the
`build` folder and build again.

### Strange CMake errors after moving or renaming the Sprout folder

Messages like *"The current CMakeCache.txt directory ... is different"* mean
the `build` folder remembers the old location. **Delete the `build` folder** and
run `cmake -S . -B build` again. Deleting `build` is always safe: it only
contains generated files. (Save files your games wrote are in there too,
though.)

### My new game doesn't show up

- The file must be directly inside its folder: `games/my_game/main.cpp`, not
  `games/my_game/src/main.cpp` and not `games/main.cpp`.
- It must end in `.cpp` (watch out for `main.cpp.txt` on Windows, where file
  extensions can be hidden).
- Run `cmake -S . -B build` again, so CMake looks for new folders. In an editor,
  use its "Configure" or "Reload CMake project" command.
- Spaces and special characters in folder names become `_` in the program's
  name: `games/space cats/` becomes `space_cats`.

### I changed my code, but the game is still the same

- Did the build finish **without errors**? If it failed, the old program is
  still there.
- Are you running the program you just built? Visual Studio's "Open folder"
  mode builds into `out\build\...`, CLion into `cmake-build-debug/`, and the
  terminal into `build/`.
- Changed a picture or sound? The `assets` folder is copied when you **build**,
  so build again.

### Windows: "cannot open file 'my_game.exe'" (LNK1168) when building

The game is still running. Close its window, then build again.

### Visual Studio starts the wrong game

- **"Open a local folder" mode:** choose your game in the **Select Startup Item**
  drop-down next to the green ▶ button.
- **`.sln` mode:** right-click your game in the **Solution Explorer** and choose
  **Set as Startup Project**. (The solution starts `my_first_game` unless you
  change it.)

---

## Compile errors

### "expected ';' after struct definition"

Every `struct` needs a semicolon after its closing curly bracket:

```cpp
struct Coin : Entity {
    // ...
};   // <- this one!
```

### "marked 'override', but does not override"

You wrote `override`, but the function doesn't match one that Sprout knows. Check
the spelling, capital letters and the brackets. The exact versions are:

```cpp
void start() override
void update(float dt) override
void draw() override
void drawUI() override      // scenes only
```

Common mistakes: `void Update(float dt)` (capital U), `void update() override`
(missing `float dt`), `void drawUi()`, or putting `drawUI()` in an entity
(entities only have `start`, `update` and `draw`).

### My `update` never runs (but there's no error)

You probably forgot `override` **and** made a typo, so C++ thinks you made a
brand-new function that nobody ever calls. Always write `override`, and the
compiler will catch it.

### "no matching function for call to 'spawn<Coin>(<brace-enclosed initializer list>)'"

Inside `spawn`, `changeScene` and `game.run`, write the type in front of curly
brackets:

```cpp
spawn<Coin>({100, 200});        // error
spawn<Coin>(Vec2{100, 200});    // works
```

### "no matching function for call to 'Star::Star()'" (or `Star::Star(float, ...)`)

The values you gave `spawn<Star>(...)` don't match Star's constructor. If the
constructor is `Star(float x, float speed)`, you must spawn it with two numbers:
`spawn<Star>(100.0f, 150.0f)`. Same for `changeScene<GameOver>(score)` and the
scene's constructor.

### "spawn<T>() only works with classes that inherit from Entity"

You forgot `: Entity` after the struct's name: `struct Coin : Entity { ... };`.
Similarly, scenes need `: Scene`.

### "'this' was not captured for this lambda function"

Add `this` to the square brackets of your lambda:

```cpp
every(1.0f, [] { spawn<Enemy>(); });       // error
every(1.0f, [this] { spawn<Enemy>(); });   // works
```

See [What is a lambda?](11-timers.md#what-is-a-lambda).

### "expected identifier before string constant" / "...before numeric constant"

You used round brackets to give a member variable its starting value. Inside a
struct, use curly brackets (or `=`):

```cpp
SaveFile save("save.json");   // error inside a struct
SaveFile save{"save.json"};   // works
Timer spawnTimer(2.0f);       // error inside a struct
Timer spawnTimer{2.0f};       // works
```

### "'Level' was not declared in this scope" / "use of undeclared identifier" / "does not name a type"

- Check the spelling and capital letters.
- C++ reads your file from top to bottom, so a struct must be written **above**
  the code that uses it. Move it up.
- **Two scenes that switch to each other** (Title to Level, and Level back to
  Title) can't both be "above" each other. Announce one function in the struct
  (`void update(float dt) override;`) and write its body **after both scenes**
  (`void Title::update(float dt) { ... changeScene<Level>(); }`). There's a
  complete example in [Scenes](04-scenes.md#when-two-scenes-need-each-other).

### "narrowing conversion of ... from 'float' to 'int'"

You put a decimal number into a whole-number variable using curly brackets.
Some compilers make this an error, others a warning. Say clearly that you want
to drop the decimals:

```cpp
int column{position.x / 32};          // narrowing!
int column = int(position.x / 32);    // fine: chops off the decimals
```

### "use of deleted function 'Entity::Entity(const Entity&)'"

You tried to **copy** an entity, for example `Enemy copy = *enemy;`. Entities
live in the scene and can't be copied. Use the pointer (`Enemy* enemy`) or a
reference (`Enemy& enemy = *pointer;`) instead.

### "reference to 'Color' is ambiguous" (or `Texture`, `Camera`, `MouseButton`, ...)

This happens when a file includes raylib (for example
`<sprout/raylib_interop.hpp>`) and also says `using namespace sprout;`. raylib
has its own types with the same names. Write `sprout::Color` (or
`raylib::Color`) to say which one you mean. See
[Using raylib-cpp directly](using-sprout-in-your-own-project.md#going-further-using-raylib-cpp-directly).

---

## Link errors

**Linking** is the last step of building, where all the compiled pieces are
glued into one program. Link errors mention `undefined reference`
(GCC/Clang), `unresolved external symbol` (Visual Studio) or `ld returned 1
exit status`.

### "undefined reference to `main'"

Your game has no `int main() { ... }`, or it's misspelled. Every game needs
exactly one.

### "undefined reference to `vtable for PlayScene'" or "unresolved external symbol ... PlayScene::update"

You announced a function (`void update(float dt) override;`) but never wrote its
body. Either add the body `{ ... }` inside the struct, or write
`void PlayScene::update(float dt) { ... }` further down. See
[the split-function trick](04-scenes.md#when-two-scenes-need-each-other).

---

## When the game runs

### The window opens and closes straight away

Look at the terminal. If it says `[Sprout] Your game stopped because of an
error:`, the next line explains what went wrong. For example:

- *"Entity::scene() was used before the entity was added to a scene"*: you used
  `scene()` (or `sceneAs`, `after` or `every`) in an entity's **constructor**.
  Move that code into `start()`.
- *"sceneAs<...>() asked for the wrong kind of scene"*: the entity lives in a
  different scene than the one between the `< >` brackets. Check the name, and
  which scene spawns that entity.
- An error mentioning JSON (`type_error`, `parse_error`): you read a value that
  isn't there or has the wrong type. See [Saving](13-saving.md#reading-json-files).

### The game crashes ("Segmentation fault", "Access violation", or it just vanishes)

The two usual causes:

1. **Using a `nullptr`.** `findFirst` and `findClosest` give `nullptr` when there
   is nothing to find. Check before using it:
   `if (Player* player = findFirst<Player>()) { ... }`.
2. **Using an entity after it was destroyed.** Don't keep entity pointers in
   member variables across frames; look entities up again each frame. See
   [Don't hold on to entity pointers](05-entities.md#dont-hold-on-to-entity-pointers).
   Inside an entity, use its **own** `after`/`every` instead of
   `scene().after(..., [this] { ... })`, because the entity's own timers stop
   when it's destroyed (see [Timers that belong to an entity](05-entities.md#timers-that-belong-to-an-entity)).

A **debugger** helps a lot here: run the game with the debugger (F5 in Visual
Studio, Ctrl + F5 in VS Code with CMake Tools) and it stops on the exact line
that crashed.

### "couldn't find the picture" and a pink and black checkerboard

Sprout couldn't find the file, so it drew a checkerboard instead. Check:

- The file is in an `assets` folder **next to your `main.cpp`**:
  `games/my_game/assets/player.png`.
- The path in your code starts with `assets/`: `loadTexture("assets/player.png")`.
- The **spelling and capital letters** match exactly. `Player.PNG` and
  `player.png` are different files on macOS and Linux.
- The file really is a `.png` (not `player.png.png`; Windows hides extensions by
  default).
- You **built again** after adding the file, so it was copied next to the game.

The same goes for sounds, fonts and JSON files.

### I can't hear anything

See [Sound and music](08-sound-and-music.md#when-you-cant-hear-anything). On
Linux, lines starting with `ALSA lib` in the terminal just mean no sound device
was found; the game still runs, silently.

### Esc closes my game

That's the default (except while the player is typing in a text box). Turn it
off (or pick another key) after creating the Game:

```cpp
setExitKey(Key::None);
```

### Everything moves way too fast (or the speed changes from computer to computer)

You forgot to multiply by `dt`:

```cpp
position.x += speed;        // 'speed' pixels EVERY FRAME: way too fast
position.x += speed * dt;   // 'speed' pixels every SECOND
```

See [Delta time](03-how-a-game-works.md#delta-time-dt).

### Everything moves really slowly (or not at all)

- With `* dt`, speeds are **per second**. A speed of 5 means 5 pixels per
  second, which is a crawl. Try numbers like 200 to 500.
- Is the scene `paused`?
- Is `update` actually running? Check for a missing `override` (see above).
- Did you `spawn` the entity?

### Something happens many times instead of once (or never)

Use `isKeyPressed` (once per press) for jumping, shooting single shots and menus.
Use `isKeyDown` (every frame while held) for walking. See
[Input](06-input.md#keys).

### "[Sprout] Warning: there is no action called ..."

The action name is misspelled, or you never created it with `bindAction`. The
ready-made actions are `"left"`, `"right"`, `"up"`, `"down"`, `"jump"` and
`"pause"`.

### Nothing I draw in `update()` shows up

Drawing only works in `draw()` and `drawUI()`. The screen is wiped right before
drawing starts, so anything drawn in `update()` disappears.

### My score moves around when the camera moves

Draw it in `drawUI()`, not `draw()`. `drawUI()` isn't affected by the camera.

### My entity is drawn half a size away from where its hitbox is

An entity's `position` is its **middle**, but `drawRect(x, y, ...)` and `Rect`
use the **top-left corner**. Draw with `drawRect(bounds(), color)`, or
`drawCircle(position, ...)`, or `drawTextureCentered(texture, position)`.

### The player falls through the floor or walks through walls

- Did you call `map.setSolid("#")` with the right letters?
- Did you call `addSolid(map)` in the scene's `start()`?
- Is the map a **member of the scene** (declared inside the struct), not a local
  variable inside `start()`?
- Does the player move with `velocity` and `moveAndSlide(dt)`, rather than
  changing `position` directly? Changing `position` yourself ignores walls.

### The player can't jump

- Is `velocity.y += Gravity * dt;` there, every frame? Without gravity pulling
  the player onto the floor, `isOnFloor()` is never true.
- Is `moveAndSlide(dt)` called every frame?
- Is the jump speed **negative** (`velocity.y = -650;`)? Negative is up.

### My save is forgotten every time

- Did you call `set`? `get` only reads.
- Saves live next to the game program, in the `build` folder
  (`save.path()` tells you exactly where). Deleting the `build` folder deletes
  them too, and a Visual Studio or CLion build has its own separate folder.
- Do you have two `SaveFile`s for the same file at the same time? They overwrite
  each other. See [Saving](13-saving.md#where-to-create-it).

### The game is slow (low frames per second)

- Show the frame rate with `drawFPS();` in `drawUI()`.
- Thousands of entities or particles? Destroy entities when they leave the
  screen, and use fewer particles.
- Debug builds are slower. Try a [Release build](01-installing.md#debug-and-release-builds).
- Don't load files with `loadJson` every frame. Load them once in `start()`.

### Text in my custom font looks blurry

Give `useFont` a bigger quality number than your biggest text size:
`useFont("assets/font.ttf", 128);`.

### My gamepad doesn't work

- Check `isGamepadConnected()` (try the input tester in [Input](06-input.md#a-complete-example-input-tester)).
- Plug it in and start the game again.
- The first controller is number 0. A second one is number 1.

---

Still stuck? Read the comment at the top of the matching header file in
`engine/include/sprout/`: each one explains how its part of Sprout works, with
examples. And compare your code with the examples in `examples/`.

---

## The Sprout Editor

### "Scripts: build failed" in the toolbar

A script has a mistake. The compiler's messages are in the **Output** panel:
click one to jump to the line. Scripts open in the Script tab show mistakes as
you type, with a wavy underline. The last version that worked keeps running
until you fix it and save.

### My script doesn't show up in Attach (or "Script 'X' ... isn't loaded")

Every script needs `REGISTER_SCRIPT(ClassName)` after the class, and must
compile. Check the toolbar's script status and the Output panel. If you renamed
the class, attach the new name to the node again.

### VS Code (or another code editor) says "cannot open source file script.hpp"

Open the project in the Sprout Editor once: it writes the files code editors
need (`compile_commands.json`, `.vscode/...`). Then open the project **folder**
in your code editor and reload its window. See
[Using another code editor](../editor/README.md#using-another-code-editor-vs-code-visual-studio).

### A signal never calls my function

- **Connected in a script:** look in the Output panel for
  `connect: 'X' (Type) has no signal 'y'`: a misspelled signal name. It lists
  the node's real signals. Also check that `getNode(...)` found the node
  (`the node is missing (nullptr)`).
- **Connected in the Inspector:** a message starting with
  `Signal connection ...` says what's wrong: the target node was renamed or
  deleted, has no script, or its script has no `CALLABLE(name)` for the
  function. The editor writes `CALLABLE` when it adds the function; if you
  renamed the function, rename it in `CALLABLE(...)` too.
- The function takes more values than the signal sends (`Signal 'x' sends 1
  value(s), but the function connected to it wants 2`).

See [Signals](../editor/README.md#signals).

### Web export: "Needs Emscripten" or not ready

Get emsdk (`git clone https://github.com/emscripten-core/emsdk.git`), pick its
folder in the Export window with **Choose...**, and press **Set Up** once. On
Windows that needs Python, and web builds need Ninja. Details:
[Exporting your game](../editor/README.md#exporting-your-game).

### Multiplayer: "Couldn't host on port 7777 (is another game using it?)"

Another program (often another copy of your game) is already hosting on that
port. Close it, or use another port.

### Multiplayer: "Couldn't connect to the server."

- Is the server running, on the same port?
- Same computer: join `127.0.0.1`. Another computer: use the host's IP address
  (`ipconfig` on Windows, `ip addr` on Linux), not `127.0.0.1`.
- Windows' firewall asks the first time a game hosts: allow it on private
  networks. Over the internet, the host's router must forward the port (UDP).

See [Playing over a network](editor/multiplayer.md#playing-over-a-network).

### A dedicated server on Linux: "error while loading shared libraries: libX11.so.6"

The game needs the small X11 library even without a window:
`sudo apt install libx11-6`.

### The extra Run Instances windows still run my old code

They keep the scripts they started with. Press **Stop** and **Play** again.
