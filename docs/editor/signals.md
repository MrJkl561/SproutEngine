# Signals, step by step

A **signal** is a node saying "something just happened": *a body walked into
me*, *my timer ran out*, *my health changed*. Any script can ask to be told
when that happens. That's called **connecting** to the signal.

Think of a doorbell. The door doesn't know who lives in the house; it just
rings. Whoever wants to know about visitors listens for the bell. Signals let
one part of your game react to another without the two knowing much about
each other.

This page builds up in four steps. If you only read one thing, read
[the one rule](#the-one-rule) just below.

---

## The one rule

Every connection answers three questions:

| Question | Example |
|---|---|
| **Which node sends it?** | the `Coin` (an `Area2D`) |
| **What's the signal called?** | `"body_entered"` |
| **Which function runs?** | `onBodyEntered`, in the script that wants to know |

In code that's one line, written **in the script that wants to know** (the
listener):

```cpp
connect(SENDER, "signal_name", FUNCTION);
```

The first thing in the brackets is always **the node that sends the signal**,
not the node that listens. That's the most common mix-up. If a Hud wants to
know when the Player's health changes, the Hud's script writes
`connect(findNode("Player"), "health_changed", ...)`: the Player sends it.

---

## Step 1: a built-in signal, connected in the editor

Some nodes send signals by themselves. An `Area2D` sends `body_entered` when a
body moves into it. Let's make a coin that disappears when the player touches
it. (This uses the Jumpy project from
[Your first game in the editor](getting-started.md), but any project with a
`CharacterBody2D` named `Player` works.)

1. Select **Main**, press **Add** and pick **Area2D**. Name it `Coin` and put
   it where the player can reach it.
2. Give `Coin` two children: a **Sprite2D** (click `coin.png` in the
   FileSystem dock) and a **CollisionShape2D** of size `24`, `24`. An Area2D
   only notices things through its CollisionShape2D, and the player needs one
   too (it has one from the tutorial).
3. Select `Coin` and, under **Script** in the Inspector, press **New**, type
   `Coin` and press Enter. The coin now has a script to receive the signal.
4. With `Coin` still selected, find **Signals** in the Inspector. It lists
   everything an Area2D can send. Next to **body_entered**, press
   **Connect...**.
5. The window asks *which node's script should be called*. Pick `Coin` itself
   (nodes without a script are greyed out). The function name is filled in
   for you: `onBodyEntered`. Press **Connect**.

The editor opens `coin.cpp` and has added this to it:

```cpp
    // Called by the "body_entered" signal of Coin (connected in the editor)
    CALLABLE(onBodyEntered);
    void onBodyEntered(Node* body) {

    }
```

Fill in what should happen:

```cpp
    CALLABLE(onBodyEntered);
    void onBodyEntered(Node* body) {
        if (body->name == "Player") node()->queueFree();  // the coin disappears
    }
```

Save (**Ctrl+S**) and press **Play**. Walk into the coin: it's gone.

What happened: you connected the Coin's `body_entered` signal to the Coin
script's `onBodyEntered` function. (Connecting to *another* node's signal
puts its name in the suggestion, like `onCoinBodyEntered`.) The connection is saved in the scene
file, so it's made again every time the game starts. The Inspector shows it
under `body_entered`: click it to jump to the function, or press **x** to
remove it.

**What's `CALLABLE` for?** A connection saved in a scene only knows the
function's *name* ("onBodyEntered"), and C++ can't find a function from its
name by itself. `CALLABLE(name)` makes that possible. You need it for
connections made **in the editor**, and only for those. If you delete the
`CALLABLE` line, the Output panel says so when the game starts.

**What's `Node* body`?** Signals can send values along. `body_entered` sends
the body that came in, so your function gets it as `body`. The Inspector
shows what each signal sends: `body_entered(Node* body)`.

## Step 2: the same thing in code

You don't have to use the Signals dock: a script can connect in its `ready()`
function. This does exactly the same as step 1 (remove the connection in the
Inspector first, with **x**, or the coin would be told twice):

```cpp
#include "script.hpp"

using namespace s2d;

class Coin : public Script {
public:
    void ready() override {
        // Sender: this script's own node (the Coin). Signal: body_entered.
        // Function: onBodyEntered, below.
        connect(node(), "body_entered", &Coin::onBodyEntered);
    }

    void onBodyEntered(Node* body) {
        if (body->name == "Player") node()->queueFree();
    }
};

REGISTER_SCRIPT(Coin)
```

- `node()` is the node this script is on: here the Coin, which sends the signal.
- `&Coin::onBodyEntered` means "the `onBodyEntered` function of this script".
  No `CALLABLE` needed, because the code names the function itself.
- Or use a lambda (a function written right there) for something short:

  ```cpp
  connect(node(), "body_entered", [this](Node* body) {
      if (body->name == "Player") node()->queueFree();
  });
  ```

Editor or code? Both work the same. The editor is handy when one node reacts
to another node's signal and you'd rather click than type paths. Code is
handy when the nodes are made while the game runs.

## Step 3: a signal of your own

Now the other direction: **your** script announces something. Say the player
has health, and a Hud shows it.

**The sender**, the Player's script, declares the signal with `SIGNAL` and
sends it with `emitSignal` whenever it happens:

```cpp
#include "script.hpp"

using namespace s2d;

class Hero : public Script {
public:
    SIGNAL(health_changed, int);  // the signal, and what it sends (an int)
    SIGNAL(died);                 // a signal that sends nothing

    EXPORT(int, health, 3);

    void hurt(int damage) {
        health -= damage;
        emitSignal("health_changed", health);  // ring the bell, with the new health
        if (health <= 0) emitSignal("died");
    }

    void process(float) override {
        if (IsKeyPressed(KEY_H)) hurt(1);  // press H to test it
    }
};

REGISTER_SCRIPT(Hero)
```

The signal belongs to the node the script is on: the `Player` node now has
`health_changed` and `died` signals, as if they were built in. They show up
under **Signals** in the Inspector too.

**The listener**, a `Label` named `Hud` with this script, connects to the
Player (the sender) in `ready()`:

```cpp
#include "script.hpp"

using namespace s2d;

class Hud : public Script {
public:
    void ready() override {
        Node* player = findNode("Player");  // the sender, found by name
        connect(player, "health_changed", &Hud::onHealthChanged);
        connect(player, "died", [this] { self<Label>()->text = "Game over"; });
    }

    void onHealthChanged(int health) {
        self<Label>()->text = "Health: " + std::to_string(health);
    }
};

REGISTER_SCRIPT(Hud)
```

Press Play and then **H**: the label counts down, and says *Game over* at 0.
The Player's script doesn't know the Hud exists. Add a sound, an enemy that
gets angry when you're hurt, or a second Hud, and the Player's script doesn't
change.

The value that `emitSignal` sends arrives as the function's parameter:
`emitSignal("health_changed", health)` → `onHealthChanged(int health)`.

## Step 4: when *not* to use a signal

Signals are for telling **other** scripts. If a script would only be
listening to itself, skip the signal and call the function:

```cpp
// Roundabout: the script signals itself
SIGNAL(happy);
void ready() override { connect(node(), "happy", &MyScript::onHappy); }
void process(float) override { if (IsKeyPressed(KEY_RIGHT)) emitSignal("happy"); }

// Simpler, and does the same
void process(float) override { if (IsKeyPressed(KEY_RIGHT)) onHappy(); }
```

Keep the `SIGNAL` (and still send it) if *other* scripts might want to know
too, or if you want to connect it to other nodes in the editor later.

---

## Common mistakes

| What you see | Why, and the fix |
|---|---|
| `connect: 'Hud' (Label) has no signal 'health_changed'` in the Output | The first thing in `connect(...)` must be the node that **sends** the signal. Here it was the Hud (the listener): use `findNode("Player")`. |
| `connect: 'Tick' (Timer) has no signal 'timout'. Its signals: ready, tree_exiting, timeout` | A typo in the signal's name. The message lists the real ones. |
| `connect('timeout'): the node is missing (nullptr)` | `getNode("...")` / `findNode("...")` didn't find the node: check its name in the Scene dock. `getNode` takes a path from this node (`"Timer"` is a child, `"../Timer"` a sibling); `findNode` looks through the whole scene. |
| `emitSignal: 'happy' isn't a signal of 'Player'` | The signal wasn't declared: add `SIGNAL(happy);` to the script that sends it. |
| The function runs 60 times a second | It's sent every frame. `IsKeyDown` is true for every frame the key is held; to send once per press, use `IsKeyPressed`. |
| `Signal connection ... has no CALLABLE(onBodyEntered)` | A connection made in the editor needs `CALLABLE(onBodyEntered);` above the function in the script, with exactly the same name. |
| It's called twice | It's connected twice: once in the editor (listed under the signal in the Inspector) and once in code. Keep one. |
| `body_entered` never comes | Both the Area2D and the body need a `CollisionShape2D` child. |

## Quick reference

```cpp
// Listen (in ready()): the sender, the signal's name, the function
connect(node(), "body_entered", &Coin::onBodyEntered);     // a function of this script
connect(getNode("Timer"), "timeout", [this] { fire(); });  // a lambda
connect(getNode("Timer"), "timeout", [this] { fire(); }, true);  // only the first time

// Stop listening
std::size_t id = connect(...);  disconnect(id);
disconnect(getNode("Timer"), "timeout");   // this script's connections to that signal
disconnect(getNode("Timer"));              // ...or to all of that node's signals

// Your own signals (in the sending script)
SIGNAL(died);                     // sends nothing
SIGNAL(health_changed, int);      // sends an int
SIGNAL(hit, Node*, float);        // sends a node and a float
emitSignal("health_changed", 2);  // send it

// Connected in the editor? The function also needs this line above it:
CALLABLE(onBodyEntered);
```

- **What a function can take:** the values the signal sends, in order:
  `Node*` (or a node type like `CharacterBody2D*`: you get `nullptr` if it
  isn't one), `int`, `float`, `bool`, `std::string`, `Vector2`, `Color`. It may
  take **fewer** than the signal sends (a lambda `[this] { ... }` is fine for
  `body_entered`), but not more.
- **Connections clean up after themselves:** when the listening script goes
  (its node is freed, the scene changes, or the script is hot reloaded), its
  connections go too, and `ready()` connects again after a reload.
- **Signals are only for this game.** To tell other players something, use an
  [RPC](multiplayer.md).

Every built-in signal, and the details of the Connect window, are in the
manual's [Signals section](../../editor/README.md#signals).
