# 1. Installing and building

Before you can make games with Sprout, you need a few free tools. This page
walks you through installing them on **Windows**, **macOS** or **Linux**, getting
Sprout, and building and running your first game.

It takes about 15 to 30 minutes, most of it waiting for downloads. You only
have to do this once.

---

## What you need, and why

| Tool | What it does |
|---|---|
| **A C++ compiler** | Turns your C++ code into a program the computer can run. This is called **compiling**. |
| **CMake** (3.25 or newer) | A **build tool**. It reads Sprout's `CMakeLists.txt` files and figures out how to compile everything, on any computer. |
| **Git** | Downloads code. You use it to get Sprout, and CMake uses it to download the libraries Sprout is built on. |
| **A code editor** (optional) | Somewhere comfortable to write code: Visual Studio, VS Code or CLion. Any text editor works too. |

You also need an **internet connection** for the first build, because CMake
downloads [raylib](https://www.raylib.com), raylib-cpp, raygui and nlohmann/json
(the open-source libraries Sprout uses) automatically.

Pick your operating system:

- [Windows](#windows)
- [macOS](#macos)
- [Linux](#linux)

---

## Windows

### 1. Install Visual Studio Community (the compiler)

Visual Studio Community is free and comes with Microsoft's C++ compiler **and**
CMake.

1. Go to <https://visualstudio.microsoft.com/vs/community/> and download
   **Visual Studio Community** (2022 or newer). Visual Studio is *not* the same
   as "Visual Studio Code", which is a separate program.
2. Run the installer. When it asks which **workloads** to install, tick
   **Desktop development with C++**.
3. On the right-hand side, make sure **C++ CMake tools for Windows** is ticked
   (it normally is).
4. Click **Install** and wait. It's a big download.

### 2. Install Git

1. Go to <https://git-scm.com/download/win> and download **Git for Windows**.
2. Run the installer. The default choices are fine, so you can keep clicking
   **Next**.

### 3. Open a terminal that knows about the compiler

The compiler and CMake from Visual Studio only work in a special terminal:

1. Open the **Start menu** and type **Developer PowerShell for VS**.
2. Click it. A blue or black window with a blinking cursor appears.

Use this window for all the commands below. To check everything is installed,
type these one at a time (press Enter after each):

```powershell
cmake --version
git --version
```

Each should print a version number. CMake must be **3.25 or newer**. If it says
something like "not recognized", see [Troubleshooting](troubleshooting.md).

> **Tip:** if you'd rather use a normal terminal, you can also install CMake
> separately from <https://cmake.org/download/>. In the installer, choose "Add
> CMake to the PATH".

Now jump to [Get Sprout](#get-sprout).

---

## macOS

### 1. Install the compiler and Git

Apple's **Command Line Tools** include a C++ compiler (Clang) and Git. Open the
**Terminal** app (search for "Terminal" with Spotlight, Cmd + Space) and type:

```bash
xcode-select --install
```

A window pops up. Click **Install** and wait. (If it says the tools are already
installed, great, you're done with this step.)

### 2. Install CMake

The easiest way is with **Homebrew**, a free installer for developer tools.

1. If you don't have Homebrew yet, follow the one-line install command on
   <https://brew.sh>.
2. Then type:

   ```bash
   brew install cmake
   ```

(Without Homebrew, you can download CMake from <https://cmake.org/download/>
instead. Open the CMake app once, then choose **Tools > How to Install For
Command Line Use** and follow what it says.)

Check that everything works:

```bash
clang++ --version
cmake --version
git --version
```

CMake must be **3.25 or newer**. Now jump to [Get Sprout](#get-sprout).

---

## Linux

Open a terminal and install the compiler, CMake, Git, and the libraries raylib
needs for windows, graphics and sound.

**Ubuntu, Debian, Linux Mint, Pop!_OS:**

```bash
sudo apt update
sudo apt install build-essential cmake git libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libasound2-dev
```

**Fedora:**

```bash
sudo dnf install gcc-c++ make cmake git libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel mesa-libGL-devel alsa-lib-devel
```

**Arch Linux, Manjaro:**

```bash
sudo pacman -S --needed base-devel cmake git libx11 libxrandr libxinerama libxcursor libxi mesa alsa-lib
```

Check the versions:

```bash
g++ --version
cmake --version
```

You need **CMake 3.25 or newer** and **GCC 11 or newer** (or Clang 14 or newer).
Older systems such as Ubuntu 22.04 come with an older CMake. In that case get a
new one with `sudo snap install cmake --classic` (or `pip install cmake`), then
close and reopen the terminal.

---

## Get Sprout

You can get Sprout with Git (recommended) or as a ZIP file.

**With Git:** in your terminal, go to the folder where you want to keep your
projects and type:

```bash
git clone https://github.com/MrJkl561/SproutEngine.git
cd SproutEngine
```

`git clone` downloads Sprout into a new folder called `SproutEngine`, and
`cd` ("change directory") moves the terminal into that folder.

**As a ZIP:** on the GitHub page, click the green **Code** button, then
**Download ZIP**. Unzip it somewhere, and in your terminal `cd` into the
unzipped folder. (You still need Git installed, because CMake uses it to
download raylib and the other libraries.)

> **Terminal tip:** `cd` moves into a folder, `cd ..` goes up one folder, and
> `ls` (macOS/Linux) or `dir` (Windows) shows what's in the current folder. On
> most systems you can also type `cd ` and then drag a folder onto the terminal
> window to paste its path.

---

## Build it

Make sure your terminal is **inside the Sprout folder** (the one with
`CMakeLists.txt` in it). Then run these two commands:

```bash
cmake -S . -B build
cmake --build build
```

1. `cmake -S . -B build` **configures** the project. `-S .` means "the source
   code is here, in this folder" (`.` means "this folder"), and `-B build` means
   "put everything you make in a folder called `build`". The `build` folder holds
   all the generated files and finished programs, and never mixes them with
   your code. If anything ever gets really confused, you can delete the `build`
   folder and run these commands again.
2. `cmake --build build` **builds** (compiles) everything: the engine, the
   examples, your games and the Sprout Editor.

**The first time takes a few minutes**, because CMake downloads raylib and the
other libraries, and then compiles raylib. You'll see a lot of text scroll by.
That's normal! After that, builds only recompile what you changed, so they take
seconds, and they work offline.

> **Faster builds:** add `--parallel` to use all your processor's cores:
> `cmake --build build --parallel`. To build just one game, name it:
> `cmake --build build --target my_first_game`.

When it's done, you should see no lines starting with `error`. Warnings are
okay.

---

## Run a game

Every game ends up in its own folder: `build/bin/<game name>/`, together with a
copy of its `assets` folder.

| | Windows | macOS / Linux |
|---|---|---|
| Your first game | `build\bin\my_first_game\my_first_game.exe` | `./build/bin/my_first_game/my_first_game` |
| An example | `build\bin\01_hello_world\01_hello_world.exe` | `./build/bin/01_hello_world/01_hello_world` |

Type the command for your system and press Enter. A window should open with a
yellow circle you can move with the arrow keys. **You just built and ran a C++
game!** Press **Esc** to close it.

You can also double-click the program in your file explorer. Sprout always
looks for `assets/...` next to the program, so it works wherever you start it
from.

All the examples are in `build/bin/` too: `01_hello_world`,
`02_shapes_and_colors`, `03_moving_around`, `04_pong` and more. Try them all,
and read their code in the `examples/` folder.

### Run the editor

The [Sprout Editor](../editor/README.md) (the visual, Godot-style part of
Sprout) is built along with everything else:

| Windows | macOS / Linux |
|---|---|
| `build\bin\sprout_editor\sprout_editor.exe` | `./build/bin/sprout_editor/sprout_editor` |

Its first build also downloads clangd (for checking scripts as you type), so
it takes a little longer. To learn it, follow
[Your first game in the editor](editor/getting-started.md).

### Build scripts

The `scripts/` folder has ready-made scripts that run these commands for you,
for example `scripts\run-editor.bat` (Windows) or `scripts/run-editor.sh`
(macOS, Linux) builds the editor and opens it, and `scripts\build.bat` /
`scripts/build.sh` builds everything. The
[main README](../README.md#build-scripts) lists them all. They use their own
build folders (`build-x64`, `build-release`...), so the programs end up there
instead of in `build/`.

---

## Building from a code editor

Typing commands works everywhere, but a code editor gives you a Build button,
error highlighting and a debugger. Pick one.

### Visual Studio (Windows)

1. Start Visual Studio and choose **Open a local folder**. Select the Sprout
   folder (the one with `CMakeLists.txt`).
2. Visual Studio notices the `CMakeLists.txt` and configures the project by
   itself. The first time, this downloads the libraries, so wait until the
   **Output** window says the CMake generation is finished.
3. At the top, find the **Select Startup Item** drop-down (next to the green
   ▶ button) and choose **my_first_game.exe** (or whichever game you want).
4. Press **Ctrl + F5** to build and run without the debugger, or **F5** to run
   with the debugger.

In this mode, Visual Studio uses its own build folder, so the program ends up
in `out\build\x64-Debug\bin\my_first_game\` instead of `build\bin\`.

> **Prefer a classic `.sln` solution?** Run `cmake -S . -B build` in the
> Developer PowerShell, then open `build\Sprout.sln` and press F5. It starts
> `my_first_game`. To run a different game, right-click it in the **Solution
> Explorer** and choose **Set as Startup Project**.

### VS Code (Windows, macOS and Linux)

1. Install [VS Code](https://code.visualstudio.com) and, on Windows, Visual
   Studio Community as described above (VS Code needs its compiler).
2. Open the Sprout folder in VS Code (**File > Open Folder...**).
3. VS Code suggests the recommended extensions, **C/C++** and **CMake Tools**.
   Click **Install**. (Or install them yourself from the Extensions panel.)
4. CMake Tools may ask you to pick a **kit**, which means a compiler. On Windows,
   choose the one that mentions **Visual Studio Community**. On macOS pick
   **Clang**, and on Linux **GCC** (or Clang).
5. Sprout's settings tell CMake Tools to configure automatically and to use the
   `build` folder, just like the terminal commands.
6. Open the **CMake** panel (the CMake icon in the left sidebar). Under
   **Launch**, pick **my_first_game**. Then use the **Build** and **Launch**
   buttons.

Handy shortcuts: **F7** builds, **Shift + F5** runs without the debugger and
**Ctrl + F5** runs with the debugger. You can also open the command palette
(**Ctrl + Shift + P**, or **Cmd + Shift + P** on a Mac) and type "CMake:" to see
every command, such as **CMake: Set Launch/Debug Target**.

### CLion (Windows, macOS and Linux)

1. Choose **Open** and select the Sprout folder. CLion recognises it as a CMake
   project and loads it (the first time takes a while because of the downloads).
2. In the run configuration drop-down at the top right, pick **my_first_game**.
3. Click the green ▶ **Run** button.

CLion uses its own build folder (`cmake-build-debug`), so the program ends up in
`cmake-build-debug/bin/my_first_game/`.

---

## Debug and Release builds

By default you get a **Debug** build. It's a little slower, but it works well
with the debugger, so it's the best choice while you're making your game.

When your game is finished, make a faster **Release** build:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
```

With Visual Studio (from the Developer PowerShell) it's
`cmake --build build --config Release` instead.

---

## Sharing your game

To give your game to a friend, copy the **whole folder**
`build/bin/<your game>/` (the program **and** its `assets` folder) and zip it up.
Use a Release build.

- **Windows:** if your friend sees an error about a missing `VCRUNTIME140.dll`,
  they need to install the free "Microsoft Visual C++ Redistributable".
- **macOS:** programs from the internet that aren't signed by Apple are blocked
  at first. Your friend can right-click the program and choose **Open**.
- Programs only run on the kind of system they were built on. A Windows `.exe`
  won't run on a Mac, so build on each system you want to support.

---

**Next:** make your own game in [Your first game](02-your-first-game.md)!
