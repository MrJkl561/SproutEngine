# Multiplayer in the Sprout Editor

The Sprout Editor can make games that several people play together over a
network, at home (LAN) or over the internet. It works like
[Godot's high-level multiplayer](https://docs.godotengine.org/en/stable/tutorials/networking/high_level_multiplayer.html):
one game **hosts** (the server) and the others **join** it, nodes are kept in
sync with a `MultiplayerSynchronizer`, players are added to everyone's game
with a `MultiplayerSpawner`, and scripts call each other's functions with
**RPCs**.

> This is for the **Sprout Editor** (`editor/`). The code-only Sprout Engine
> doesn't have networking.

**Contents**

- [Try the template](#try-the-template)
- [How it fits together](#how-it-fits-together)
- [Hosting and joining](#hosting-and-joining)
- [Multiplayer authority](#multiplayer-authority-who-controls-what)
- [MultiplayerSynchronizer](#multiplayersynchronizer-keep-values-the-same-everywhere)
- [MultiplayerSpawner](#multiplayerspawner-add-things-to-everyones-game)
- [RPCs](#rpcs-call-a-function-on-other-players-games)
- [Prediction: no waiting for the server](#prediction-no-waiting-for-the-server)
- [Lag compensation: hit what you see](#lag-compensation-hit-what-you-see)
- [Testing: Run Instances](#testing-run-instances)
- [Dedicated servers](#dedicated-servers)
- [Playing over a network](#playing-over-a-network)
- [Godot names](#for-godot-users)
- [Limits](#limits)

---

## Try the template

1. **Projects** → **New Project...**, type a name, pick **Multiplayer** under
   *Start from*, and press **Create**.
2. Press **Play**. Two copies of the game start: one in the editor (it hosts)
   and one in a window of its own (it joins).
3. Click into either one and play: **arrow keys** move, **Space** jumps,
   **E** waves to the others, **F** shoots. Your player moves the instant you
   press a key (prediction), the server decides where everyone really is,
   and shots count where you saw the others (lag compensation).
4. To feel a slow connection, open the window button next to **Stop** and add
   `--lag 150` to copy 2's command line. It still moves at once.
5. Press **Stop**: both copies stop.

The template's two scripts, `scripts/lobby.cpp` and `scripts/net_player.cpp`,
are short and full of comments. They're the best place to start reading.

## How it fits together

```
          the server (peer 1)                      a client (peer 2)
  +-------------------------------+        +-------------------------------+
  | Main  (Lobby script)          |        | Main  (Lobby script)          |
  | ├── Players                   |        | ├── Players                   |
  | │   ├── Player1  (authority 1)| -----> | │   ├── Player1   <- copied   |
  | │   └── Player2  (authority 2)| <----- | │   └── Player2   moves here  |
  | └── MultiplayerSpawner        |        | └── MultiplayerSpawner        |
  +-------------------------------+        +-------------------------------+
```

- Every game runs the **same scenes and scripts**.
- The **server** decides who's playing: when someone joins, it adds a `Player`
  for them below `Players`. The **MultiplayerSpawner** copies that player into
  every client's game, with the same name.
- Each player has a **multiplayer authority**: the peer it belongs to. Only
  that game reads the keyboard for it.
- In the template, players use [prediction](#prediction-no-waiting-for-the-server):
  the player's own game moves at once and sends its keys to the server, the
  server makes the real move, and each player's **MultiplayerSynchronizer**
  sends the server's result to all the others.
- Everything goes through the server. Clients never talk to each other
  directly; the server passes messages on.

Every game has a **peer id**: the server is always **1**, and clients get
**2, 3, 4...** in the order they join.

## Hosting and joining

From any script:

```cpp
multiplayer().host(7777);                    // be the server, on port 7777
multiplayer().join("192.168.1.20", 7777);    // join a server (an IP address or computer name)
multiplayer().close();                       // leave, or stop hosting
```

`host()` returns `false` if it can't (another program is using the port), and
`multiplayer().lastError` says why. `join()` returns at once; one of these is
called on every script when it's done:

```cpp
void connectedToServer() override { }       // joined! multiplayer().uniqueId() is our id now
void connectionFailed() override { }        // nobody answered (after a few seconds)
void serverDisconnected() override { }      // the server stopped, or the connection was lost
void peerConnected(int id) override { }     // someone joined
void peerDisconnected(int id) override { }  // someone left
```

`peerConnected` is called on the server for each client that joins, and on
clients for the server (1) and every other client, including the ones that
were already there when you joined.

Other things to ask:

| | |
|---|---|
| `multiplayer().isServer()` | This game is the server (also `true` when not networked at all) |
| `multiplayer().isConnected()` | Hosting, or joined and accepted |
| `multiplayer().isActive()` | Hosting, joined, or still joining |
| `multiplayer().uniqueId()` | This game's peer id (1 for the server) |
| `multiplayer().peers()` | The ids of everyone else |

**The command line.** Games can be started with arguments, like
`MyGame --join 192.168.1.20`. Scripts read them with `hasArg("--host")` and
`argValue("--join")` (the word after it), or all of them with
`commandLineArgs()`. The template uses `--host`, `--join <address>` and
`--server`.

## Multiplayer authority: who controls what

Every node has a multiplayer authority, a peer id. It's **1** (the server)
unless you change it:

```cpp
player->setMultiplayerAuthority(id);   // that peer controls this node (and the nodes inside it)
node()->multiplayerAuthority();        // who it is
isMultiplayerAuthority()               // in a script: is it us?
```

The rule of thumb: **only the authority changes a node; everyone else just
shows it.** So a simple player script starts like this (for players, the
better way is [prediction](#prediction-no-waiting-for-the-server), where
the server makes the real moves):

```cpp
void process(float delta) override {
    if (!isMultiplayerAuthority()) return;  // someone else's player: the synchronizer moves it
    // read the keyboard, move...
}
```

When the game isn't networked, `isMultiplayerAuthority()` is `true` for
nodes with authority 1, so single-player testing just works. The authority
isn't saved in scene files: set it in code, usually right after making the
node, before adding it to the scene.

## MultiplayerSynchronizer: keep values the same everywhere

Add a `MultiplayerSynchronizer` inside the node to keep in sync, and list
what to sync in its **properties** field, separated by commas:

| Write | Syncs |
|---|---|
| `position` | the parent node's `position` |
| `position, velocity` | several properties |
| `Sprite:flip_h` | `flip_h` of the node `Sprite` inside it |
| `:health` | the parent's script variable `health` (one made with `EXPORT`) |
| `Gun:ammo` | the script variable `ammo` of the script on `Gun` |

The names are the ones the Inspector shows. Its other fields:

- **root_path**: the node whose properties are synced, from the
  synchronizer. `..` (its parent) unless you change it.
- **interval**: seconds between updates. `0` = every frame (60 a second).
  `0.05` = 20 a second: much less network traffic, and with interpolation it
  still looks smooth.
- **interpolate** (on unless you turn it off): other games glide the node
  between updates instead of jumping. See below.
- **interpolation_delay**: how far in the past (seconds) other games show the
  node, `0.1` unless you change it. See below.

The node's **authority** sends its values; everyone else takes them (for a
[predicted](#prediction-no-waiting-for-the-server) node, the server sends them
instead). Values
that haven't changed aren't sent again (except once a second, in case one got
lost). Updates are sent the fast way: if one is lost, the next one replaces
it. The server checks that updates from a client really come from that
node's authority, so a client can't move other people's players.

### Smooth movement (interpolation)

Updates arrive a few times a second, and never exactly on time. Shown as they
come, other players would jump and stutter. So each game shows other players'
nodes **a moment in the past** (the **interpolation_delay**, 0.1 seconds),
where it already has the updates from just before and just after, and blends
between them every frame. The result moves smoothly even at 20 updates a
second, or when some arrive late.

- **Numbers, positions and colors** are blended (`position`, `velocity`,
  `rotation`, `modulate`, a `float` script variable...).
- **Everything else** (`flip_h`, text, `int` and `bool` values) changes
  when the moment of its update is reached, in step with the movement.
- Your **own** nodes (the ones you're the authority of) are never delayed:
  they move the instant you press a key.
- **Bigger delay** = smoother on bad connections, but others are shown a bit
  further behind. **Smaller** = more up to date, but late updates show as
  stutter. 0.1 is right for most games; try 0.15 to 0.2 over the internet.
- If a node stands still, nothing is sent; when it moves again it picks up
  smoothly from where it stopped.
- Turn **interpolate** off for things that must show the latest value right
  away, or that jump on purpose all the time.

## MultiplayerSpawner: add things to everyone's game

Add a `MultiplayerSpawner` to the scene and set its **spawn_path** to the
node that things are added to (`../Players` in the template). Then, on the
**server**, add scenes there the normal way:

```cpp
auto player = instantiate("scenes/player.scene.json");
player->name = "Player" + std::to_string(id);   // a name that's the same everywhere
player->setMultiplayerAuthority(id);
getNode("Players")->addChild(std::move(player));
```

The spawner sends it to every client: same name, same properties, same
script variable values from the Inspector, same authority. When the server frees it
(`queueFree()`), it goes from every game. Players who join later get
everything that was spawned before they came.

- Only nodes **added while the game runs** are copied. Nodes saved in the
  scene file are already in everyone's game.
- Only the **server** spawns. Things a client adds there stay in that game.
- Give each spawned node its own name. If two have the same name, the
  spawner adds a number to the second one.
- On clients the spawner sends the [signals](../../editor/README.md#signals)
  `spawned(Node* node)` when a copy arrives and `despawned(Node* node)` just
  before one goes, to connect to like any other signal.

## RPCs: call a function on other players' games

An RPC (remote procedure call) runs one of your script's functions in the
other games. Mark the function with `RPC(name)`:

```cpp
class NetPlayer : public Script {
public:
    RPC(wave);
    void wave() {
        // show a speech bubble...
    }

    RPC(takeDamage);
    void takeDamage(int amount, Vector2 from) { ... }
};
```

Then call it:

```cpp
rpc("wave");                          // on every other game's copy of this node
rpcId(1, "takeDamage", 10, Vector2{1, 0});   // only on the server (peer 1)
rpcId(3, "wave");                     // only on peer 3
```

- It runs on the **same node** in the other games, found by its path from the
  scene's root (that's why spawned nodes need the same names everywhere).
- `rpc()` doesn't run it here. To run it here too, call the function
  yourself as well (the template does `rpc("wave"); wave();`).
- Arguments can be `bool`, `int`, `float`, `std::string`, `Vector2` and
  `Color` (raylib-cpp's too, like `body->position`), up to as many as you
  like. They arrive in order, and every RPC arrives (they're never lost).
- RPCs are for other games. Within one game, use
  [signals](../../editor/README.md#signals).
- Inside the function, `rpcSender()` is the id of the peer that called it.
- **Any peer can call any RPC.** For things only the server should decide
  (damage, scores, who won), check who's asking:

```cpp
RPC(requestScore);
void requestScore(int points) {
    if (!multiplayer().isServer()) return;   // only the server keeps score
    // rpcSender() says which player asked
}
```

## Prediction: no waiting for the server

There are two ways to move a player:

| | **The player's game decides** | **The server decides, with prediction** |
|---|---|---|
| How | The player's game moves it and sends the position | The player's game sends its **keys**; the server moves it |
| Your own player | Moves at once | Moves at once (predicted) |
| Cheating | A modified game can put its player anywhere | Can't: the server only takes keys |
| Bumping into others | Each game sees its own version | The server's version counts |
| Code | `process()` with `isMultiplayerAuthority()` | `INPUT`, `gatherInput()`, `simulate()` |

For players, the second is better, and it's what the template does. Mark
what the player controls with `INPUT`, read the keyboard in
`gatherInput()`, and move in `simulate()`:

```cpp
class NetPlayer : public Script {
public:
    EXPORT(float, speed, 220);

    // What the player presses: sent to the server 60 times a second
    INPUT(float, moveX, 0);
    INPUT(bool, jump, false);

    // On the player's own game only: read the keyboard into the INPUTs
    void gatherInput() override {
        moveX = (IsKeyDown(KEY_RIGHT) ? 1.0f : 0.0f) - (IsKeyDown(KEY_LEFT) ? 1.0f : 0.0f);
        jump = IsKeyDown(KEY_SPACE);
    }

    // On the server (the real move) and the player's own game (the prediction)
    void simulate(float delta) override {
        auto* body = self<CharacterBody2D>();
        body->velocity.x = moveX * speed;
        body->velocity.y += 1300 * delta;
        if (jump && body->isOnFloor()) body->velocity.y = -520;
        body->moveAndSlide();
    }
};
```

Give the node a `MultiplayerSynchronizer` listing what `simulate()` changes
(`position, velocity`), and set its multiplayer authority to the player's peer
id as before. Then, 60 times a second:

1. The player's own game calls `gatherInput()`, sends the inputs to the
   server, and calls `simulate()` straight away: **the player moves with no
   delay**. It keeps the inputs it sent.
2. The server calls `simulate()` with each input as it arrives: **that's the
   real move**. Its synchronizer sends the result to everyone, saying which
   input it got up to.
3. Everyone else shows the server's result, [interpolated](#smooth-movement-interpolation).
4. The player's own game takes the server's result and **replays** the inputs
   the server hadn't got to yet on top of it. Usually that lands exactly where
   it already was, so nothing changes on screen. If the server saw something
   different (someone was in the way), the player is quietly put right.

Things to know:

- `INPUT` works with `float`, `int`, `bool` and `Vector2`. Use `IsKeyDown`
  (held) in `gatherInput()` rather than `IsKeyPressed`: it runs 60 times a
  second, not once per frame, so a press could fall between two calls.
- `simulate()` runs at exactly 60 ticks a second everywhere, whatever the
  frame rate, with the same `delta`. Keep **everything that moves** the
  player in it, and nothing else: the same inputs must give the same moves.
- During a replay, `multiplayer().isReplaying()` is `true`: don't play
  sounds or make effects in `simulate()` then (or keep them in `process()`).
- `process()` still runs every frame on every game, for animations, labels
  and so on. `isMultiplayerAuthority()` is still how a script knows it's the
  player's own game (e.g. to send an RPC when a key is pressed).
- The server only takes inputs from the player whose node it is, and ignores
  positions sent by clients for predicted nodes.
- `multiplayer().predictionError()` says how far (pixels) the last correction
  moved your player: 0 almost always. `multiplayer().ping()` is the round trip
  to the server in milliseconds.
- When the game isn't networked, or on the server's own player, the inputs
  are simply used straight away.

## Lag compensation: hit what you see

Other players are shown a moment in the past (interpolation, plus the time
their moves took to reach you). So when you shoot at someone, by the time
the server hears about it they've moved on, and you'd miss what you aimed at.

**Lag compensation** fixes that. Every RPC a client sends says what moment
that client was seeing. On the server, inside the RPC, `lagCompensated()`
runs your check with **every synced node moved back to where that client
saw it**, then puts them back:

```cpp
// On the player's own game, when F is pressed
rpcId(1, "shoot", body->position, dir);

// On the server
RPC(shoot);
void shoot(Vector2 from, float dir) {
    if (!multiplayer().isServer()) return;
    Node* victim = nullptr;
    lagCompensated([&] {
        // ...look for a player whose CollisionShape2D the shot crosses...
    });
    if (victim) { /* count the hit, tell everyone */ }
}
```

The template's `shoot()` in `net_player.cpp` is a complete example.

- It moves back the nodes that have a `MultiplayerSynchronizer` (where their
  synchronizer's parent was), except the shooter's own: the shot starts from
  where the shooter is now.
- The server keeps one second of history, and never goes back more than half
  a second (so a cheater can't claim to have seen the distant past).
- Outside a client's RPC (on the server's own player, or when not networked)
  it just runs your check as things are.

## Testing: Run Instances

To test a multiplayer game you need several copies of it running. The
**window button** next to **Stop** (or **gear** → **Run Instances...**)
sets how many copies **Play** starts, from 1 to 4:

- Copy 1 runs in the editor, as always.
- The others open in windows of their own, side by side. Their `print()`
  messages show in the Output panel, marked `[2]`, `[3]`...
- Each copy has its own command line. The template uses `--host` for copy 1
  and `--join 127.0.0.1` for copy 2. Scripts read them with `hasArg()` and
  `argValue()`.
- `--server` makes a copy a **dedicated server** with no window (see below).
- `--lag 150` makes a copy's network slow: 150 ms added to the round trip.
  Add `--jitter 30` for uneven delays and `--loss 5` to lose 5% of the
  updates. Great for seeing prediction, interpolation and lag compensation
  at work without a second computer.
- **Stop** stops them all.

This is saved in the project's `project.json`. The extra windows use the
scripts as they were when you pressed Play: after changing a script, press
Stop and Play again to see it in all of them.

The editor's own multiplayer is tested on every build with three real games
(a server and two clients, no windows), once on a perfect network and once
with `--lag 120 --jitter 20 --loss 5`: see `scripts/test-multiplayer.sh`
(`scripts\test-multiplayer.bat` on Windows) and `editor/tests/multiplayer/`.
It checks, among the rest, that prediction is exact (0 px of correction),
that a client can't move its predicted player by itself, and that a
lag-compensated shot finds the target where the client saw it (within a
pixel, against about 25 px off without compensation).

## Dedicated servers

A **dedicated server** is a copy of the game that only hosts: no window, no
player of its own, nothing to draw. It's what you'd leave running on a
computer (or a rented server) for others to join.

Every exported game for Windows, Linux or macOS can be one: start it with
`--server`:

```bash
./MyGame --server            # Linux / macOS
MyGame.exe --server          # Windows
```

It runs without a window, printing to the terminal it was started from, until
you press **Ctrl+C**. Your scripts decide what
to do; `hasArg("--server")` tells them. The template hosts without adding a
player of its own. `isHeadless()` is `true` too, and pictures aren't loaded.

- In the editor, give a Run Instances copy the argument `--server`.
- From the command line, without exporting:
  `sprout_editor --project "My Game" --play --server` (everything after
  `--play` goes to the game; put the game's own arguments after `--`, e.g.
  `--play --server -- --lag 100`).
- On a Linux server with no desktop, the game still needs the small X11
  library to start: `sudo apt install libx11-6`.

## Playing over a network

- **Same computer:** join `127.0.0.1`.
- **Same home network (LAN):** the host finds its IP address (Windows:
  `ipconfig`; macOS / Linux: `ip addr` or the network settings, something
  like `192.168.1.20`), and the others join that. The first time a game
  hosts on Windows, the firewall asks whether to allow it: allow it on
  private networks.
- **Over the internet:** the host's router must forward the game's port
  (UDP, 7777 in the template) to the host's computer, and the others join
  the host's public IP address. Or run a dedicated server somewhere that has
  a public address.

Sprout's multiplayer uses UDP through [ENet](https://github.com/lsalzman/enet),
the library Godot uses too. It's downloaded with the editor's other libraries.

## For Godot users

| Godot | Sprout Editor |
|---|---|
| `ENetMultiplayerPeer.create_server(port)` | `multiplayer().host(port)` |
| `ENetMultiplayerPeer.create_client(ip, port)` | `multiplayer().join(ip, port)` |
| `multiplayer.get_unique_id()` | `multiplayer().uniqueId()` |
| `multiplayer.is_server()` | `multiplayer().isServer()` |
| `multiplayer.get_peers()` | `multiplayer().peers()` |
| `multiplayer.get_remote_sender_id()` | `rpcSender()` |
| `peer_connected` / `peer_disconnected` signals | `peerConnected(id)` / `peerDisconnected(id)` |
| `connected_to_server` / `connection_failed` / `server_disconnected` | `connectedToServer()` / `connectionFailed()` / `serverDisconnected()` |
| `@rpc("any_peer")` | `RPC(name);` |
| `rpc("f", ...)` / `rpc_id(id, "f", ...)` | `rpc("f", ...)` / `rpcId(id, "f", ...)` |
| `set_multiplayer_authority(id)` / `is_multiplayer_authority()` | `setMultiplayerAuthority(id)` / `isMultiplayerAuthority()` |
| `MultiplayerSynchronizer` (replication config) | `MultiplayerSynchronizer` (`properties` field) |
| `MultiplayerSpawner` (spawn path + auto spawn list) | `MultiplayerSpawner` (`spawn_path`) |
| `MultiplayerSpawner.spawned` / `despawned` signals | the same signals, `spawned` / `despawned` |
| Debug → Customize Run Instances | the window button next to Stop |
| (addons like netfox) rollback / prediction | `INPUT`, `gatherInput()`, `simulate()` |
| `--headless` dedicated server export | any desktop export, started with `--server` |

## Limits

- **Not in web games.** Browsers can't use UDP. In a web export, `host()`
  and `join()` return `false` and the game plays alone.
- **Prediction is for your own player.** Things others push around (a ball
  you kick) are moved by the server and shown interpolated, so your effect on
  them shows up after a round trip.
- **Lag compensation moves positions only** (of nodes with a synchronizer),
  not other properties, and only inside a client's RPC on the server.
- **RPCs can be called by anyone.** Check `rpcSender()` and
  `multiplayer().isServer()` where it matters.
- **Scene changes aren't synced.** If the server calls `changeScene()`, tell
  the clients with an RPC so they change too.
- **Up to 32 players** per server by default (`host(port, maxClients)`
  changes it).
