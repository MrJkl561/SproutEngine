// The game's start: hosts or joins a game, and adds a player for everyone who
// connects. It's on the Main node of scenes/main.scene.json.
//
// How the multiplayer works:
// - One game hosts: it's the server, peer 1. The others join it.
// - The server adds a Player for each peer (itself too, unless it's a
//   dedicated server) and makes that peer its multiplayer authority.
// - The MultiplayerSpawner copies every player the server adds below "Players"
//   to all the other games, including ones that join later.
// - Each Player's MultiplayerSynchronizer sends its position from the game that
//   controls it to everyone else (see net_player.cpp).
//
// Press Play: Run Instances (the window button next to Stop) starts two
// copies, one with --host and one with --join 127.0.0.1. Or start them by hand
// and press H to host or J to join.
#include "script.hpp"

using namespace s2d;

class Lobby : public Script {
public:
    EXPORT(int, port, 7777);
    EXPORT(std::string, address, "127.0.0.1");  // where J joins
    EXPORT_SCENE(playerScene, "scenes/player.scene.json");

    void ready() override {
        // Already connected: a script was just hot reloaded while playing
        if (multiplayer().isActive()) return showPlayers();
        // The command line (Run Instances, or an exported game's)
        if (hasArg("--server")) startServer(false);  // a dedicated server: no player of its own
        else if (hasArg("--host")) startServer(true);
        else if (hasArg("--join")) joinGame(argValue("--join", address));
        else setStatus("Press H to host a game, or J to join " + address);
    }

    void process(float) override {
        if (multiplayer().isActive()) return;
        if (IsKeyPressed(KEY_H)) startServer(true);
        if (IsKeyPressed(KEY_J)) joinGame(address);
    }

    // --- the server -------------------------------------------------------
    void startServer(bool withPlayer) {
        if (!multiplayer().host(port)) {
            setStatus("Couldn't host: " + multiplayer().lastError);
            return;
        }
        if (withPlayer) addPlayer(1);
        showPlayers();
    }

    void addPlayer(int id) {
        auto player = instantiate(playerScene);
        if (!player) return;
        player->name = "Player" + std::to_string(id);  // the same name in every game
        player->setMultiplayerAuthority(id);            // that peer controls it
        if (auto* body = player->as<Node2D>()) body->position = Vector2{160.0f + 120.0f * static_cast<float>(id % 5), 100};
        getNode("Players")->addChild(std::move(player));  // the spawner copies it to everyone
    }

    void peerConnected(int id) override {
        if (multiplayer().isServer()) addPlayer(id);
        showPlayers();
    }

    void peerDisconnected(int id) override {
        if (multiplayer().isServer())
            if (Node* player = getNode("Players/Player" + std::to_string(id))) player->queueFree();
        showPlayers();
    }

    // --- a client ---------------------------------------------------------
    void joinGame(const std::string& where) {
        if (multiplayer().join(where, port)) setStatus("Joining " + where + " ...");
        else setStatus("Couldn't join: " + multiplayer().lastError);
    }

    void connectedToServer() override { showPlayers(); }
    void connectionFailed() override { setStatus("No game found at " + address + ". H: host, J: try again"); }
    void serverDisconnected() override {
        print("The server closed the game.");
        reloadScene();  // back to the start
    }

    // --- the status line --------------------------------------------------
    void showPlayers() {
        int count = static_cast<int>(multiplayer().peers().size()) + 1;
        std::string me = multiplayer().isServer() ? "the server" : "player " + std::to_string(multiplayer().uniqueId());
        setStatus("You are " + me + ". Players online: " + std::to_string(count));
    }

    void setStatus(const std::string& text) {
        if (auto* label = findNode<Label>("Status")) label->text = text;
        print(text);
    }
};

REGISTER_SCRIPT(Lobby)
