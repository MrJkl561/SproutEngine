// Multiplayer, modelled on Godot's high-level multiplayer API.
//
// One game hosts (the server, peer id 1) and the others join it (clients, peer
// ids 2, 3, ...). Every message goes through the server. From a script:
//
//     multiplayer().host(7777);                  // be the server
//     multiplayer().join("127.0.0.1", 7777);     // or join one
//
//     void peerConnected(int id) override { ... }   // someone joined (see Script)
//
//     RPC(jump);                                 // jump() may be called over the network
//     void jump(float height) { ... }
//     rpc("jump", 300.0f);                       // run jump(300) on every other peer
//
// Two nodes do the rest without code:
//   MultiplayerSynchronizer - keeps properties (position, ...) the same everywhere
//   MultiplayerSpawner      - copies scenes the server adds below a node to everyone
//
// Every node has a multiplayer authority: the peer whose copy is the real one
// (1, the server, unless set). Synchronizers send the authority's values, and
// scripts check isMultiplayerAuthority() before reading the keyboard.
//
// Built on ENet (UDP). Not available in web exports: browsers can't use UDP.
#pragma once

#include <raylib-cpp.hpp>

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace s2d {

class Node;
class SceneTree;

// What can be sent in an RPC: the same types EXPORT shows.
using NetValue = std::variant<bool, int, float, std::string, Vector2, Color>;
using NetArgs = std::vector<NetValue>;

class Multiplayer {
public:
    Multiplayer();
    ~Multiplayer();
    Multiplayer(const Multiplayer&) = delete;
    Multiplayer& operator=(const Multiplayer&) = delete;

    // Be the server, waiting for players on this port. False (and lastError)
    // if it can't, e.g. the port is in use.
    bool host(int port = 7777, int maxClients = 32);
    // Join a server: an IP address or a computer name. Returns at once;
    // connectedToServer() or connectionFailed() tells scripts how it went.
    bool join(const std::string& address = "127.0.0.1", int port = 7777);
    // Leave (a client) or stop (the server, disconnecting everyone).
    void close();

    bool isActive() const;        // hosting, or joined / joining
    bool isServer() const;        // hosting, or not networked at all (like Godot)
    bool isConnected() const;     // hosting, or a client that has been accepted
    int uniqueId() const;         // this game's peer id: 1 = the server (and offline)
    std::vector<int> peers() const;  // the other peers (a client sees the server and the other clients)
    int remoteSenderId() const;   // while an RPC runs: the peer that sent it (0 otherwise)
    std::string lastError;

    // Used by Script::rpc / rpcId. target: 0 = every other peer, else one peer.
    void sendRpc(const Node* node, const std::string& method, const NetArgs& args, int target);

    // Called by the SceneTree every frame: poll() before scripts run (messages
    // in, events to scripts), flush() after (synchronizers and spawners out).
    void poll(SceneTree& tree);
    void flush(SceneTree& tree);

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

// The game's multiplayer (one per program, kept across scene changes).
Multiplayer& multiplayer();

// The command line the game was started with, without the program's name,
// e.g. {"--join", "192.168.1.20"}. In the editor: the Run Instances arguments.
const std::vector<std::string>& commandLineArgs();
void setCommandLineArgs(std::vector<std::string> args);
bool hasArg(const std::string& arg);                    // hasArg("--server")
std::string argValue(const std::string& arg, const std::string& fallback = "");  // argValue("--join") -> what follows it
// Running without a window: a dedicated server (started with --server).
bool isHeadless();
void setHeadless(bool headless);

}  // namespace s2d
