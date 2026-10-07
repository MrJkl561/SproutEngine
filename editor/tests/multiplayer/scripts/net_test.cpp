// The multiplayer test (scripts/test-multiplayer.sh, also run by CI): one
// dedicated server and two clients, each its own program with no window:
//
//   sprout_editor --project editor/tests/multiplayer --play --server --frames 900
//   sprout_editor --project editor/tests/multiplayer --play --headless --frames 780 -- --expect-leave
//   sprout_editor --project editor/tests/multiplayer --play --headless --frames 360   (a bit later)
//
// Each one prints "CHECK <name> OK" for everything that worked, and
// "SERVER PASSED" / "CLIENT PASSED" once all of its checks have.
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "script.hpp"

using namespace s2d;

class NetTest : public Script {
public:
    void ready() override {
        int port = std::stoi(argValue("--port", "7790"));
        if (hasArg("--server")) {
            server = true;
            expected = {"hosting", "ping_from_2", "ping_from_3", "sync_from_Player2", "sync_from_Player3",
                        "script_var_from_Player2", "server_saw_disconnect"};
            if (multiplayer().host(port)) check("hosting");
            else print("FAILED to host: " + multiplayer().lastError);
            // Something the server owns and moves, for the clients to see
            auto ball = instantiate("scenes/ball.scene.json");
            getNode("Players")->addChild(std::move(ball));
        } else {
            leaves = hasArg("--expect-leave");  // stays long enough to see the other client go
            expected = {"connected", "rpc_roundtrip", "own_player_spawned", "server_sync", "interpolated", "prediction", "server_authoritative",
                        "lag_compensation", "other_player_spawned",
                        "client_sync_relayed", "script_var_sync", "rpc_broadcast_relayed", "peer_connected"};
            if (leaves) {
                expected.push_back("peer_disconnected");
                expected.push_back("despawned");
            }
            multiplayer().join("127.0.0.1", port);
        }
    }

    // --- events --------------------------------------------------------------
    void peerConnected(int id) override {
        if (server) {
            auto player = instantiate("scenes/test_player.scene.json");
            player->name = "Player" + std::to_string(id);
            player->setMultiplayerAuthority(id);  // that client moves it
            getNode("Players")->addChild(std::move(player));
        } else if (id != 1) {
            other = id;
            check("peer_connected");
            rpc("hello", multiplayer().uniqueId());  // to everyone else, through the server
        }
    }
    void peerDisconnected(int id) override {
        if (server) {
            if (Node* n = getNode("Players/Player" + std::to_string(id))) n->queueFree();
            check("server_saw_disconnect");
        } else if (id == other) {
            check("peer_disconnected");
            otherLeft = true;
        }
    }
    void connectedToServer() override {
        check("connected");
        rpcId(1, "ping", 21, std::string("hi"), raylib::Vector2{1, 2}, 1.5f);  // (raylib-cpp's Vector2, like body->position)
    }
    void connectionFailed() override { print("FAILED to connect"); }

    // --- RPCs ----------------------------------------------------------------
    RPC(ping);
    void ping(int value, std::string text, Vector2 where, float f) {
        if (server && value == 21 && text == "hi" && where.x == 1 && where.y == 2 && f == 1.5f) {
            check("ping_from_" + std::to_string(rpcSender()));
            rpcId(rpcSender(), "pong", value * 2);
        }
    }
    RPC(pong);
    void pong(int value) {
        if (value == 42 && rpcSender() == 1) check("rpc_roundtrip");
    }
    // Server: is the ball where the client saw it, after rewinding?
    RPC(shootAt);
    void shootAt(float seenX) {
        auto* ball = dynamic_cast<Node2D*>(getNode("Players/Ball"));
        if (!server || !ball) return;
        float without = std::abs(ball->position.x - seenX), with = 1e9f;
        lagCompensated([&] { with = std::abs(ball->position.x - seenX); });
        rpcId(rpcSender(), "shotResult", without, with);
    }
    RPC(shotResult);
    void shotResult(float without, float with) {
        print("shot missed by " + std::to_string(without) + " px without lag compensation, " + std::to_string(with) +
              " px with");
        if (with < 2 && without > 5) check("lag_compensation");
    }

    RPC(hello);
    void hello(int from) {
        if (!server && from == rpcSender() && from == other) check("rpc_broadcast_relayed");
    }

    // --- every frame ---------------------------------------------------------
    void process(float delta) override {
        Node* players = getNode("Players");
        if (server) {
            if (auto* ball = dynamic_cast<Node2D*>(players->getNode("Ball"))) ball->position.x += 120 * delta;
            for (auto& c : players->children()) {
                auto* p = dynamic_cast<Node2D*>(c.get());
                if (!p || p->name == "Ball") continue;
                if (p->position.x > 30) check("sync_from_" + p->name);
                if (p->script && p->name == "Player2") {
                    Exports e;
                    p->script->collectExports(e);
                    if (!e.list.empty() && *static_cast<int*>(e.list[0].ptr) > 30) check("script_var_from_Player2");
                }
            }
        } else if (multiplayer().isConnected()) {
            int me = multiplayer().uniqueId();
            ++connectedFrames;
            auto* mine = dynamic_cast<Node2D*>(players->getNode("Player" + std::to_string(me)));
            // Prediction: our player moves at once, and the server's answers agree with it
            if (mine && connectedFrames == 150) {
                print("prediction error " + std::to_string(multiplayer().predictionError()) + " px, at x " +
                      std::to_string(mine->position.x));
                if (mine->position.x > 30 && multiplayer().predictionError() < 0.5f) check("prediction");
            }
            // The server decides: moving our own player by hand (cheating) is undone
            if (mine && connectedFrames == 160) mine->position.x = 5000;
            if (mine && connectedFrames == 220 && mine->position.x < 4000) check("server_authoritative");
            // Lag compensation: "shoot" at the ball where we see it (in the past)
            if (connectedFrames >= 100 && connectedFrames <= 400 && connectedFrames % 30 == 0)
                if (auto* ball = dynamic_cast<Node2D*>(players->getNode("Ball")))
                    rpcId(1, "shootAt", ball->position.x);
            if (Node* mine = players->getNode("Player" + std::to_string(me)); mine && mine->isMultiplayerAuthority())
                check("own_player_spawned");
            if (auto* ball = dynamic_cast<Node2D*>(players->getNode("Ball")); ball && ball->position.x > 10) {
                check("server_sync");
                // The server sends the ball 20 times a second, but interpolation
                // should move it a little every frame, and never backwards.
                if (ballFrames < 120) {
                    float x = ball->position.x;
                    if (ballFrames > 0 && x != lastBallX) ++ballMoves;
                    if (ballFrames > 0 && x < lastBallX - 0.001f) ++ballBackwards;
                    lastBallX = x;
                    if (++ballFrames == 120) {
                        print("ball moved on " + std::to_string(ballMoves) + " of 119 frames, backwards on " +
                              std::to_string(ballBackwards));
                        if (ballMoves >= 100 && ballBackwards == 0) check("interpolated");
                    }
                }
            }
            if (other) {
                auto* theirs = dynamic_cast<Node2D*>(players->getNode("Player" + std::to_string(other)));
                if (theirs) {
                    check("other_player_spawned");
                    if (theirs->position.x > 20 && !theirs->isMultiplayerAuthority()) check("client_sync_relayed");
                    if (theirs->script) {
                        Exports e;
                        theirs->script->collectExports(e);
                        if (!e.list.empty() && *static_cast<int*>(e.list[0].ptr) > 20) check("script_var_sync");
                    }
                } else if (otherLeft) {
                    check("despawned");
                }
            }
        }
    }

private:
    void check(const std::string& name) {
        if (!passed.insert(name).second) return;
        print("CHECK " + name + " OK");
        for (const std::string& e : expected)
            if (!passed.count(e)) return;
        if (!done) print(server ? "SERVER PASSED" : "CLIENT PASSED");
        done = true;
    }

    bool server = false, leaves = false, otherLeft = false, done = false;
    int other = 0;
    int connectedFrames = 0;
    int ballFrames = 0, ballMoves = 0, ballBackwards = 0;
    float lastBallX = 0;
    std::set<std::string> passed;
    std::vector<std::string> expected;
};

REGISTER_SCRIPT(NetTest)
