Sprout Editor (portable)
========================

This folder is a complete Sprout Editor. Move it or copy it anywhere: another
folder, another drive, a USB stick or another computer. Keep everything in it
together.

Start it with sprout_editor (sprout_editor.exe on Windows).

What's in here
  sprout_editor    the editor
  editor/          its sources (your scripts are compiled against them), the
                   project templates and the demo project
  cmake/, deps/    the libraries, for building scripts and exporting games

Your projects aren't stored in here: new ones go in SproutProjects in your home
folder. Each project's script and export builds go in your user cache folder
(Windows: %LOCALAPPDATA%\SproutEditor; macOS: ~/Library/Caches/SproutEditor;
Linux: ~/.cache/sprout-editor), so this folder can even be read-only.

On another computer
  The editor compiles your C++ scripts while it runs, so the computer needs the
  same kind of tools the editor was built with:
    Windows: Visual Studio (any edition) with "Desktop development with C++",
             and CMake (Visual Studio's own copy is found too)
    macOS:   Xcode's command line tools and CMake (brew install cmake ninja)
    Linux:   g++ and CMake (and ninja, to build faster)
  It also has to be the same kind of computer: an editor built for Windows x64
  runs on Windows x64, and so on.

Games you export don't need any of this: they run on their own.

More: https://github.com/MrJkl561/SproutEngine  (editor/README.md)
