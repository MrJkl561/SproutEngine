# 8. Sound and music

Sound makes a huge difference. A jump with a "boing", a coin with a "ding" and
some background music make even a simple game feel alive.

Sprout knows two kinds of sound:

| | Sound effects | Music |
|---|---|---|
| Length | Short (a jump, a hit, a coin) | Long (a background song) |
| How many at once | Lots, even the same one several times | One song at a time |
| How it's loaded | All at once, into memory | Bit by bit while it plays ("streaming") |
| Functions | `playSound(...)` | `playMusic(...)` |

**File types:** `.wav`, `.ogg`, `.mp3`, `.flac` (and `.qoa`). Use `.wav` or
`.ogg` for sound effects, and `.ogg` or `.mp3` for music (they're much smaller).

**Where to put them:** in your game's `assets` folder, next to `main.cpp`, just
like pictures (see [Drawing](07-drawing.md#where-to-put-your-pictures)). Then
use paths like `"assets/jump.wav"`.

You never have to load or unload sounds yourself. Sprout loads each file the
first time you use it and remembers it.

---

## Sound effects

```cpp
playSound("assets/jump.wav");                 // play it
playSound("assets/coin.wav", 0.5f);           // at half volume
playSound("assets/hit.wav", 1.0f, 1.5f);      // higher and faster
```

| Code | What it does |
|---|---|
| `playSound(path, volume = 1, pitch = 1)` | Plays a sound. Volume: 0 = silent, 1 = normal. Pitch: 1 = normal, 2 = higher and faster, 0.5 = lower and slower. |
| `stopSound(path)` | Stops every copy of that sound that's playing |
| `isSoundPlaying(path)` | Is that sound playing right now? |
| `preloadSound(path)` | Loads a sound now, so there's no tiny delay the first time it plays |

The same sound can **overlap itself**, so rapid fire sounds right: every shot
gets its own "pew", even if the last one hasn't finished. (Up to 8 copies of one
sound play at once; after that the oldest copy is cut off.)

> **Pro tip:** play the same sound at a slightly different pitch each time, and
> it won't sound so repetitive. Players hear the difference, even if they can't
> say what it is:
>
> ```cpp
> playSound("assets/coin.wav", 1.0f, randomFloat(0.9f, 1.1f));
> ```

---

## Music

```cpp
playMusic("assets/song.ogg");            // plays and loops forever
playMusic("assets/boss.ogg", 0.7f);      // at 70% volume
playMusic("assets/win.ogg", 1.0f, false); // play once, don't loop
```

| Code | What it does |
|---|---|
| `playMusic(path, volume = 1, loop = true)` | Starts a song. A new song replaces the old one. |
| `stopMusic()` | Stops the music |
| `pauseMusic()` | Pauses the music (it remembers where it was) |
| `resumeMusic()` | Carries on after `pauseMusic()` |
| `isMusicPlaying()` | Is music playing (and not paused)? |
| `setMusicVolume(volume)` | Changes the volume of the current song |

Some useful things to know:

- **Music keeps playing when you change scenes.** Start your song in the title
  screen and it carries on into the game.
- Calling `playMusic` with the song that's **already playing does nothing** (it
  doesn't restart from the beginning). So it's safe to put
  `playMusic("assets/song.ogg");` in the `start()` of every scene that should have
  that song. Coming back to that scene won't make the music jump, and a volume
  you set with `setMusicVolume` stays as it is.
- To give each level its own song, just call `playMusic` with a different file
  in each level's `start()`.

Pause the music when the game is paused:

```cpp
if (isActionPressed("pause")) {
    paused = !paused;
    if (paused) pauseMusic();
    else resumeMusic();
}
```

---

## Volume for everything

```cpp
setMasterVolume(0.5f);       // everything at half volume
float now = masterVolume();  // what is it right now?
```

The master volume affects all sound effects and music together. It's perfect for
a volume slider in a settings menu (see [Menus and UI](12-menus-and-ui.md)), and
you can [save](13-saving.md) it so it's remembered next time.

---

## A complete example: a sound board

This needs three files in the `assets` folder: `jump.wav`, `coin.wav` and
`song.ogg`. Use any sounds you like, just change the names to match.

```cpp
#include <sprout/sprout.hpp>

using namespace sprout;

struct SoundBoard : Scene {
    void start() override {
        background = Color::DarkPurple;
        preloadSound("assets/jump.wav");   // no tiny delay the first time
        playMusic("assets/song.ogg", 0.5f); // half volume, loops forever
    }

    void update(float) override {
        if (isKeyPressed(Key::One)) playSound("assets/jump.wav");
        if (isKeyPressed(Key::Two)) playSound("assets/coin.wav", 1.0f, randomFloat(0.8f, 1.2f));

        if (isKeyPressed(Key::M)) {
            if (isMusicPlaying()) pauseMusic();
            else resumeMusic();
        }

        // Up and down arrows change the volume of everything
        if (isKeyPressed(Key::Up)) setMasterVolume(clamp(masterVolume() + 0.1f, 0, 1));
        if (isKeyPressed(Key::Down)) setMasterVolume(clamp(masterVolume() - 0.1f, 0, 1));
    }

    void drawUI() override {
        drawText("1: jump sound    2: coin sound (random pitch)", 20, 20, 24);
        drawText("M: pause / resume the music", 20, 60, 24);
        drawText("Up / Down: volume " + std::to_string(int(masterVolume() * 100)) + "%", 20, 100, 24);
        drawText(isMusicPlaying() ? "Music: playing" : "Music: paused", 20, 140, 24, Color::Gold);
    }
};

int main() {
    Game game("Sound board", 800, 450);
    game.run<SoundBoard>();
}
```

`clamp(value, 0, 1)` keeps the volume between 0 and 1. Also have a look at the
example `examples/10_sprites_and_sound`.

---

## When you can't hear anything

- **Check the terminal.** If a file can't be found or loaded, Sprout prints a
  warning like
  `[Sprout] Warning: couldn't find the sound "assets/jump.wav"`. Check the
  spelling (capital letters count on macOS and Linux) and that the file really is
  in the `assets` folder next to your `main.cpp`. Then **build again**, so the
  folder gets copied next to your game.
- **No sound card?** If the computer has no working sound device, Sprout prints
  `no sound device was found, so the game will be silent` and all the sound
  functions quietly do nothing. Your game still runs fine.
- **Volume:** check `setMasterVolume`, the volume you passed in, and your
  computer's volume.

---

## Where to get sounds

- **[jsfxr](https://sfxr.me)**: make retro sound effects (jumps, coins,
  explosions, power-ups) in your browser with one click. Download them as
  `.wav`.
- **[Kenney's audio packs](https://kenney.nl/assets?q=audio)**: free sound
  effects you can use for anything (CC0).
- **[OpenGameArt](https://opengameart.org)** and
  **[freesound](https://freesound.org)**: lots of free sounds and music. Check
  the license of each one, because some ask you to credit the creator.
- **[Audacity](https://www.audacityteam.org)**: a free program to cut, change
  and convert sounds (for example to `.ogg`).

**Next:** [Levels and physics](09-levels-and-physics.md).
