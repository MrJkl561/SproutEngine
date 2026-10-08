#include "multiplayer.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <map>
#include <set>

#include "net_transport.hpp"
#include "scene_io.hpp"
#include "scene_tree.hpp"
#include "script.hpp"

namespace s2d {

using nlohmann::json;

namespace {

// ---------------------------------------------------------------------------
// The command line
// ---------------------------------------------------------------------------
std::vector<std::string>& argList() {
    static std::vector<std::string> args;
    return args;
}
bool gHeadless = false;

// ---------------------------------------------------------------------------
// RPC arguments <-> JSON (sent as CBOR, a compact binary JSON)
// ---------------------------------------------------------------------------
json toJson(const NetValue& v) {
    return std::visit(
        [](const auto& x) -> json {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, Vector2>) return {{"v2", {x.x, x.y}}};
            else if constexpr (std::is_same_v<T, Color>) return {{"c", {x.r, x.g, x.b, x.a}}};
            else return x;
        },
        v);
}

NetValue fromJson(const json& j) {
    if (j.is_boolean()) return j.get<bool>();
    if (j.is_number_integer()) return j.get<int>();
    if (j.is_number()) return j.get<float>();
    if (j.is_string()) return j.get<std::string>();
    if (j.is_object() && j.contains("v2")) return Vector2{j["v2"][0].get<float>(), j["v2"][1].get<float>()};
    if (j.is_object() && j.contains("c"))
        return Color{j["c"][0].get<unsigned char>(), j["c"][1].get<unsigned char>(), j["c"][2].get<unsigned char>(),
                     j["c"][3].get<unsigned char>()};
    return 0;
}

// Every script below root, for sending events to (collected first: scripts may
// add nodes while handling one).
std::vector<Node*> scriptedNodes(Node* root) {
    std::vector<Node*> out;
    forEachNode(root, [&](Node* n) {
        if (n->script) out.push_back(n);
    });
    return out;
}

// The properties a synchronizer keeps in sync, each named as written in its
// list ("position", "Sprite2D:flip_h", ":health"), pointing into the nodes.
std::vector<Property> syncedProperties(const MultiplayerSynchronizer& sync, Node* target) {
    std::vector<Property> out;
    size_t start = 0;
    const std::string& list = sync.properties;
    while (start <= list.size()) {
        size_t comma = list.find(',', start);
        std::string entry = list.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        start = comma == std::string::npos ? list.size() + 1 : comma + 1;
        entry.erase(0, entry.find_first_not_of(" \t"));
        entry.erase(entry.find_last_not_of(" \t") + 1);
        if (entry.empty()) continue;

        size_t colon = entry.find(':');
        std::string path = colon == std::string::npos ? "" : entry.substr(0, colon);
        std::string name = colon == std::string::npos ? entry : entry.substr(colon + 1);
        Node* n = target->getNode(path);
        if (!n) continue;
        std::vector<Property> props;
        if (colon == std::string::npos || !path.empty()) n->getProperties(props);
        auto it = std::find_if(props.begin(), props.end(),
                               [&](const Property& p) { return p.name == name && p.kind != PropKind::Section; });
        if (it == props.end() && n->script) {  // a script variable
            Exports e;
            n->script->collectExports(e);
            props = e.list;
            it = std::find_if(props.begin(), props.end(), [&](const Property& p) { return p.name == name; });
        }
        if (it == props.end()) continue;
        Property p = *it;
        p.name = entry;
        out.push_back(p);
    }
    return out;
}

// Seconds on this computer's steady clock (synchronizer updates carry it)
double now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// The synced values a fraction f of the way from a to b: numbers, positions
// and colors are blended; anything else keeps a's value until b is reached.
json blend(const std::vector<Property>& props, const json& a, const json& b, double f) {
    json out = a;
    for (const Property& p : props) {
        if (!a.contains(p.name) || !b.contains(p.name)) continue;
        const json &x = a[p.name], &y = b[p.name];
        auto mix = [&](double u, double v) { return u + (v - u) * f; };
        if (p.kind == PropKind::Float && x.is_number() && y.is_number()) {
            out[p.name] = mix(x.get<double>(), y.get<double>());
        } else if ((p.kind == PropKind::Vec2 || p.kind == PropKind::Color) && x.is_array() && y.is_array() &&
                   x.size() == y.size()) {
            json v = json::array();
            for (size_t i = 0; i < x.size(); ++i) {
                double m = mix(x[i].get<double>(), y[i].get<double>());
                if (p.kind == PropKind::Color) v.push_back(static_cast<int>(std::lround(m)));
                else v.push_back(m);
            }
            out[p.name] = v;
        }
    }
    return out;
}

// --lag <ms> [--jitter <ms>] [--loss <percent>] on the command line: pretend
// the network is slow, to see how the game plays over the internet.
void applyLagArgs(net::Transport& transport) {
    int lag = std::atoi(argValue("--lag", "0").c_str());
    int jitter = std::atoi(argValue("--jitter", "0").c_str());
    float loss = static_cast<float>(std::atof(argValue("--loss", "0").c_str()));
    transport.simulateLag(lag, jitter, loss);
    if (lag > 0 || jitter > 0 || loss > 0)
        print("Simulating a slow network: " + std::to_string(lag) + " ms lag, " + std::to_string(jitter) +
              " ms jitter, " + std::to_string(static_cast<int>(loss)) + "% lost");
}

// Predicted scripts tick at a fixed rate, the same on every game, so the
// server and the player's own game make exactly the same moves.
constexpr double kTick = 1.0 / 60.0;

// The node's script, if it's moved by its inputs (has INPUT variables)
Script* predictedScript(Node* node) {
    return node && node->script && node->script->isPredicted() ? node->script.get() : nullptr;
}

raylib::Vector2 positionOf(Node* node) {
    auto* n2d = dynamic_cast<Node2D*>(node);
    return n2d ? n2d->position : raylib::Vector2{0, 0};
}

}  // namespace

const std::vector<std::string>& commandLineArgs() { return argList(); }
void setCommandLineArgs(std::vector<std::string> args) { argList() = std::move(args); }
bool hasArg(const std::string& arg) {
    return std::find(argList().begin(), argList().end(), arg) != argList().end();
}
std::string argValue(const std::string& arg, const std::string& fallback) {
    auto& a = argList();
    auto it = std::find(a.begin(), a.end(), arg);
    if (it == a.end() || it + 1 == a.end()) return fallback;
    return *(it + 1);
}
bool isHeadless() { return gHeadless; }
void setHeadless(bool headless) { gHeadless = headless; }

Multiplayer& multiplayer() {
    static Multiplayer instance;
    return instance;
}

// ---------------------------------------------------------------------------
// The connection
// ---------------------------------------------------------------------------
struct Multiplayer::Impl {
    enum class Mode { Off, Server, Client };
    Mode mode = Mode::Off;
    bool accepted = false;  // a client the server has welcomed
    int id = 1;
    int sender = 0;
    std::set<int> others;   // a client's view of the other peers
    std::set<int> clients;  // the server's clients
    net::Transport transport;

    // Events for scripts, sent at the end of poll()
    enum class Ev { PeerConnected, PeerDisconnected, Connected, Failed, ServerGone };
    std::vector<std::pair<Ev, int>> events;

    // What each spawner has copied to the clients (server only)
    struct Spawned {
        std::vector<std::string> order;    // names, oldest first
        std::map<std::string, Node*> nodes;
    };
    std::map<std::string, Spawned> spawners;  // by the spawner's path
    Node* spawnRoot = nullptr;                 // the scene they belong to

    // Interpolation: the latest updates of each synchronizer this game shows
    // (but doesn't own), timed by the clock of the game that sent them.
    struct Snapshot {
        double time;
        json values;
    };
    struct Buffer {
        std::string path;  // to notice when the node is gone
        int sender = 0;
        std::deque<Snapshot> snaps;
    };
    std::map<MultiplayerSynchronizer*, Buffer> buffers;
    std::map<int, double> clockOffset;  // per sender: our clock minus theirs, at the quickest delivery
    double viewTime = -1;  // client: the moment (server clock) it shows the server's nodes at

    // Prediction. Our own predicted nodes (client): the inputs the server
    // hasn't answered yet, to replay on top of its answer.
    struct Predicted {
        int seq = 0;      // the number of our latest input
        int lastAck = 0;  // the latest input the server has answered
        std::deque<std::pair<int, json>> history;
    };
    std::map<std::string, Predicted> predicted;  // by node path
    // The server: inputs from players, waiting for their tick
    struct Inputs {
        std::map<int, json> pending;  // by input number
        int lastProcessed = 0;
    };
    std::map<std::string, Inputs> inputs;  // by node path
    double tickTime = 0;
    bool replaying = false;
    float predError = 0;

    // Lag compensation (server): where the synced nodes were, the last second
    struct Pose {
        Node2D* node;
        std::string path;
        raylib::Vector2 position;
    };
    struct Frame {
        double time;
        std::vector<Pose> poses;
    };
    std::deque<Frame> history;
    double rpcViewTime = -1;  // while a client's RPC runs: when that client sent it, in what it saw

    void reset() {
        mode = Mode::Off;
        accepted = false;
        id = 1;
        sender = 0;
        others.clear();
        clients.clear();
        spawners.clear();
        spawnRoot = nullptr;
        buffers.clear();
        clockOffset.clear();
        viewTime = -1;
        predicted.clear();
        inputs.clear();
        history.clear();
        predError = 0;
        transport.close();
    }

    // Messages travel as CBOR (a compact binary form of JSON).
    void send(int peer, const json& msg, bool reliable) { transport.send(peer, json::to_cbor(msg), reliable); }
    // Server: to every client but one (0 = all)
    void broadcast(const json& msg, bool reliable, int except = 0) {
        std::vector<std::uint8_t> bytes = json::to_cbor(msg);
        for (int c : clients)
            if (c != except) transport.send(c, bytes, reliable);
    }

    void runRpc(Node* root, const std::string& path, const std::string& method, const json& args, int from,
                double sentViewTime = -1) {
        Node* node = root ? root->getNode(path) : nullptr;
        if (!node || !node->script) return;  // not in this game's scene (yet)
        NetArgs list;
        for (const json& a : args) list.push_back(fromJson(a));
        int before = sender;
        double beforeView = rpcViewTime;
        sender = from;
        rpcViewTime = sentViewTime;
        if (!node->script->callRpc(method, list))
            print("RPC '" + method + "' isn't marked RPC(" + method + ") in script " + node->scriptName);
        sender = before;
        rpcViewTime = beforeView;
    }

    // An update from `origin` (the game that owns the node) for a synchronizer
    void receiveSync(Node* root, const json& msg, int origin) {
        std::string path = msg.value("p", "");
        auto* sync = root ? dynamic_cast<MultiplayerSynchronizer*>(root->getNode(path)) : nullptr;
        Node* target = sync ? sync->getNode(sync->rootPath) : nullptr;
        if (!target) return;
        if (Script* script = predictedScript(target)) {
            if (mode == Mode::Server) return;  // the server makes predicted moves itself
            if (target->multiplayerAuthority() == id) return reconcile(*sync, *target, *script, root, msg);
            // someone else's: shown smoothly, like any other
        } else if (target->isMultiplayerAuthority()) {
            return;
        }
        json values = msg.value("v", json::object());
        if (!sync->interpolate || !msg.contains("ts")) {
            buffers.erase(sync);
            applyPropertyList(syncedProperties(*sync, target), values);
            return;
        }
        // How far behind their clock ours is. The quickest delivery is the
        // truest, so it only goes down... except very slowly, for clock drift.
        double ts = msg["ts"].get<double>();
        double offset = now() - ts;
        auto it = clockOffset.find(origin);
        if (it == clockOffset.end() || offset < it->second) clockOffset[origin] = offset;
        else it->second += (offset - it->second) * 0.001;

        Buffer& b = buffers[sync];
        if (b.path != path) b = Buffer{path, origin, {}};
        b.sender = origin;
        if (!b.snaps.empty()) {
            if (ts <= b.snaps.back().time) return;  // older than what we have (arrived out of order)
            // Unchanged values aren't sent: after a pause, it stood still until just before this one
            double step = std::max(static_cast<double>(sync->interval), 1.0 / 60.0);
            if (ts - b.snaps.back().time > step * 3 + 0.05) b.snaps.push_back({ts - step, b.snaps.back().values});
        }
        b.snaps.push_back({ts, std::move(values)});
        while (b.snaps.size() > 64) b.snaps.pop_front();
    }

    // Every frame: shows each interpolated node where it was interpolationDelay ago.
    void interpolateAll(Node* root) {
        double t = now();
        for (auto it = buffers.begin(); it != buffers.end();) {
            MultiplayerSynchronizer* sync = it->first;
            Buffer& b = it->second;
            // Gone (freed, or another scene), or ours now
            Node* target = root && root->getNode(b.path) == sync ? sync->getNode(sync->rootPath) : nullptr;
            if (!target || target->isMultiplayerAuthority() || !sync->interpolate || b.snaps.empty()) {
                it = buffers.erase(it);
                continue;
            }
            double show = t - clockOffset[b.sender] - std::max(0.0f, sync->interpolationDelay);
            if (b.sender == 1) viewTime = show;  // for lag compensation: what we see of the server's world
            while (b.snaps.size() >= 2 && b.snaps[1].time <= show) b.snaps.pop_front();
            auto props = syncedProperties(*sync, target);
            if (b.snaps.size() == 1 || show <= b.snaps[0].time) {
                applyPropertyList(props, b.snaps[0].values);  // waiting for the next update: stay put
            } else {
                const Snapshot &from = b.snaps[0], &to = b.snaps[1];
                double f = std::clamp((show - from.time) / (to.time - from.time), 0.0, 1.0);
                applyPropertyList(props, blend(props, from.values, to.values, f));
            }
            ++it;
        }
    }

    // Our own predicted node: the server's answer arrived. Take the server's
    // state, then replay the inputs it hasn't seen yet on top of it.
    void reconcile(MultiplayerSynchronizer& sync, Node& target, Script& script, Node* root, const json& msg) {
        Predicted& p = predicted[pathBetween(root, &target)];
        int ack = msg.value("ack", 0);
        if (ack <= p.lastAck) return;  // an older answer (arrived out of order)
        p.lastAck = ack;
        raylib::Vector2 before = positionOf(&target);
        applyPropertyList(syncedProperties(sync, &target), msg.value("v", json::object()));
        if (auto* body = dynamic_cast<CharacterBody2D*>(&target); body && msg.contains("c") && msg["c"].size() == 3)
            body->setContacts(msg["c"][0].get<bool>(), msg["c"][1].get<bool>(), msg["c"][2].get<bool>());
        while (!p.history.empty() && p.history.front().first <= ack) p.history.pop_front();
        json latest = propertyListToJson(script.inputs());
        replaying = true;
        for (auto& [seq, in] : p.history) {
            applyPropertyList(script.inputs(), in);
            script.simulate(static_cast<float>(kTick));
        }
        replaying = false;
        applyPropertyList(script.inputs(), latest);
        predError = positionOf(&target).Distance(before);
    }

    // One fixed tick of every predicted script.
    void tick(Node* root) {
        std::vector<Node*> nodes;
        forEachNode(root, [&](Node* n) {
            if (predictedScript(n) && !n->isQueuedForDeletion()) nodes.push_back(n);
        });
        std::set<std::string> live;
        int me = mode == Mode::Off ? 1 : id;
        for (Node* n : nodes) {
            Script* script = n->script.get();
            if (!script || !script->isPredicted()) continue;  // (changed by an earlier one)
            int owner = n->multiplayerAuthority();
            std::string path = pathBetween(root, n);
            live.insert(path);
            if (owner == me && mode != Mode::Client) {
                // Ours, and we're the server (or not networked): nothing to predict
                script->gatherInput();
                script->simulate(static_cast<float>(kTick));
            } else if (owner == me && accepted) {
                // Ours, on a client: move now, send the input, remember it for replaying
                script->gatherInput();
                Predicted& p = predicted[path];
                json in = propertyListToJson(script->inputs());
                p.history.push_back({++p.seq, in});
                while (p.history.size() > 120) p.history.pop_front();
                // The last few inputs each time, so one lost packet loses nothing
                json list = json::array();
                for (size_t i = p.history.size() > 6 ? p.history.size() - 6 : 0; i < p.history.size(); ++i)
                    list.push_back({p.history[i].first, p.history[i].second});
                send(1, {{"t", "in"}, {"p", path}, {"i", list}}, false);
                script->simulate(static_cast<float>(kTick));
            } else if (mode == Mode::Server) {
                // A client's: one move per input it sent, exactly as its own game
                // made them (if one is late, it waits: guessing would undo the prediction)
                Inputs& in = inputs[path];
                int moves = in.pending.size() > 6 ? 3 : 1;  // catch up if they piled up
                for (int i = 0; i < moves && !in.pending.empty(); ++i) {
                    auto first = in.pending.begin();
                    applyPropertyList(script->inputs(), first->second);
                    in.lastProcessed = first->first;
                    in.pending.erase(first);
                    script->simulate(static_cast<float>(kTick));
                }
            }
            // else someone else's, on a client: shown from the server's answers
        }
        for (auto it = predicted.begin(); it != predicted.end();) it = live.count(it->first) ? std::next(it) : predicted.erase(it);
        for (auto it = inputs.begin(); it != inputs.end();) it = live.count(it->first) ? std::next(it) : inputs.erase(it);
    }

    // Server: a client's inputs for its predicted node
    void receiveInputs(Node* root, const json& msg, int from) {
        std::string path = msg.value("p", "");
        Node* node = root ? root->getNode(path) : nullptr;
        if (!predictedScript(node) || node->multiplayerAuthority() != from) return;  // not theirs to move
        Inputs& in = inputs[path];
        for (const json& e : msg.value("i", json::array())) {
            if (!e.is_array() || e.size() != 2) continue;
            int seq = e[0].get<int>();
            if (seq > in.lastProcessed && in.pending.size() < 120) in.pending.emplace(seq, e[1]);
        }
    }

    // Server, every frame: where every synced node is, for lag compensation
    void recordHistory(Node* root) {
        Frame f{now(), {}};
        forEachNode(root, [&](Node* n) {
            if (auto* sync = dynamic_cast<MultiplayerSynchronizer*>(n))
                if (auto* target = dynamic_cast<Node2D*>(sync->getNode(sync->rootPath)))
                    f.poses.push_back({target, pathBetween(root, target), target->position});
        });
        history.push_back(std::move(f));
        while (!history.empty() && history.front().time < history.back().time - 1.0) history.pop_front();
    }

    static Node* spawnParent(Node* root, const std::string& spawnerPath) {
        auto* spawner = root ? dynamic_cast<MultiplayerSpawner*>(root->getNode(spawnerPath)) : nullptr;
        return spawner ? spawner->getNode(spawner->spawnPath) : nullptr;
    }

    // A message from `from` (on a client, always the server, 1).
    void handle(Node* root, const json& msg, int from) {
        std::string t = msg.value("t", "");
        if (mode == Mode::Client) {
            int origin = msg.value("from", 1);
            if (t == "welcome") {
                id = msg.value("id", 0);
                accepted = true;
                events.push_back({Ev::Connected, id});
                others = {1};
                events.push_back({Ev::PeerConnected, 1});
                for (int p : msg.value("peers", std::vector<int>{})) {
                    others.insert(p);
                    events.push_back({Ev::PeerConnected, p});
                }
            } else if (t == "peer+") {
                others.insert(msg.value("id", 0));
                events.push_back({Ev::PeerConnected, msg.value("id", 0)});
            } else if (t == "peer-") {
                others.erase(msg.value("id", 0));
                events.push_back({Ev::PeerDisconnected, msg.value("id", 0)});
            } else if (t == "rpc") {
                runRpc(root, msg.value("p", ""), msg.value("m", ""), msg.value("a", json::array()), origin);
            } else if (t == "sync") {
                receiveSync(root, msg, origin);
            } else if (t == "spawn") {
                Node* parent = spawnParent(root, msg.value("p", ""));
                const json& n = msg["n"];
                std::string name = n.value("name", "");
                if (!parent || name.empty() || parent->getNode(name)) return;  // nowhere to put it, or have it already
                auto node = nodeFromJson(n);
                if (!node) return;
                node->setMultiplayerAuthority(msg.value("auth", 1));
                Node* added = parent->addChild(std::move(node));
                if (Node* spawner = root->getNode(msg.value("p", ""))) spawner->emitSignal("spawned", added);
            } else if (t == "despawn") {
                Node* parent = spawnParent(root, msg.value("p", ""));
                std::string name = msg.value("name", "");
                if (Node* n = parent && !name.empty() ? parent->getNode(name) : nullptr; n && n != parent) {
                    if (Node* spawner = root->getNode(msg.value("p", ""))) spawner->emitSignal("despawned", n);
                    n->queueFree();
                }
            }
            return;
        }

        // The server
        if (t == "rpc") {
            int to = msg.value("to", 0);
            json out = msg;
            out.erase("to");
            out["from"] = from;
            out.erase("vt");
            double vt = msg.value("vt", -1.0);
            if (to == 0) {
                runRpc(root, msg.value("p", ""), msg.value("m", ""), msg.value("a", json::array()), from, vt);
                broadcast(out, true, from);
            } else if (to == 1) {
                runRpc(root, msg.value("p", ""), msg.value("m", ""), msg.value("a", json::array()), from, vt);
            } else if (clients.count(to)) {
                send(to, out, true);
            }
        } else if (t == "in") {
            receiveInputs(root, msg, from);
        } else if (t == "sync") {
            // Only the node's authority may change it
            auto* sync = root ? dynamic_cast<MultiplayerSynchronizer*>(root->getNode(msg.value("p", ""))) : nullptr;
            Node* target = sync ? sync->getNode(sync->rootPath) : nullptr;
            if (!target || target->multiplayerAuthority() != from || predictedScript(target)) return;
            receiveSync(root, msg, from);
            json out = msg;  // (keeps the sender's clock, "ts", for the others' interpolation)
            out["from"] = from;
            broadcast(out, false, from);
        }
    }

    // Server: everything spawned so far, to a peer that just joined.
    void catchUp(Node* root, int peerId) {
        if (!clients.count(peerId) || root != spawnRoot) return;
        for (auto& [path, spawned] : spawners) {
            for (const std::string& name : spawned.order) {
                Node* n = spawned.nodes[name];
                send(peerId, {{"t", "spawn"}, {"p", path}, {"n", nodeToJson(*n)}, {"auth", n->multiplayerAuthority()}},
                     true);
            }
        }
    }

    // Server: copies new children of each spawner's node to the clients, and
    // removes the ones that were freed.
    void updateSpawner(Node* root, MultiplayerSpawner& spawner) {
        Node* parent = spawner.getNode(spawner.spawnPath);
        if (!parent) return;
        std::string path = pathBetween(root, &spawner);
        Spawned& s = spawners[path];
        std::set<Node*> present;
        for (auto& c : parent->children()) {
            Node* n = c.get();
            if (!n->addedWhilePlaying || n->isQueuedForDeletion()) continue;
            present.insert(n);
            auto it = s.nodes.find(n->name);
            if (it != s.nodes.end() && it->second == n) continue;  // sent already
            if (it != s.nodes.end()) {
                // Two spawned nodes with one name: clients couldn't tell them apart
                int i = 2;
                while (s.nodes.count(n->name + std::to_string(i))) ++i;
                n->name += std::to_string(i);
            }
            s.nodes[n->name] = n;
            s.order.push_back(n->name);
            broadcast({{"t", "spawn"}, {"p", path}, {"n", nodeToJson(*n)}, {"auth", n->multiplayerAuthority()}}, true);
        }
        for (auto it = s.nodes.begin(); it != s.nodes.end();) {
            if (present.count(it->second)) {
                ++it;
                continue;
            }
            broadcast({{"t", "despawn"}, {"p", path}, {"name", it->first}}, true);
            s.order.erase(std::remove(s.order.begin(), s.order.end(), it->first), s.order.end());
            it = s.nodes.erase(it);
        }
    }

    void updateSynchronizer(Node* root, MultiplayerSynchronizer& sync, float delta) {
        Node* target = sync.getNode(sync.rootPath);
        if (!target) return;
        Script* predictedBy = predictedScript(target);
        // Predicted nodes are moved by the server, so it sends them; others, their authority
        if (predictedBy ? mode != Mode::Server : !target->isMultiplayerAuthority()) return;
        sync.sinceSent += delta;
        if (sync.sinceSent < std::max(0.0f, sync.interval)) return;
        json values = propertyListToJson(syncedProperties(sync, target));
        int ack = 0;
        json contacts;
        if (predictedBy) {
            ack = inputs.count(pathBetween(root, target)) ? inputs[pathBetween(root, target)].lastProcessed : 0;
            if (auto* body = dynamic_cast<CharacterBody2D*>(target))
                contacts = {body->isOnFloor(), body->isOnWall(), body->isOnCeiling()};
        }
        std::string text = values.dump() + std::to_string(ack);
        // Unchanged values are sent again now and then, in case one got lost.
        if (text == sync.lastSent && sync.sinceSent < 1.0) return;
        sync.sinceSent = 0;
        sync.lastSent = text;
        json msg = {{"t", "sync"}, {"p", pathBetween(root, &sync)}, {"v", values}, {"ts", now()}};
        if (predictedBy) {
            msg["ack"] = ack;  // the last input of its player that this state includes
            if (!contacts.is_null()) msg["c"] = contacts;
        }
        if (mode == Mode::Server) broadcast(msg, false);
        else send(1, msg, false);
    }
};

Multiplayer::Multiplayer() : impl_(std::make_unique<Impl>()) {}
Multiplayer::~Multiplayer() { close(); }

bool Multiplayer::host(int port, int maxClients) {
    close();
    lastError.clear();
    if (!impl_->transport.host(port, maxClients, lastError)) return false;
    applyLagArgs(impl_->transport);
    impl_->mode = Impl::Mode::Server;
    impl_->id = 1;
    return true;
}

bool Multiplayer::join(const std::string& address, int port) {
    close();
    lastError.clear();
    if (!impl_->transport.connect(address, port, lastError)) return false;
    applyLagArgs(impl_->transport);
    impl_->mode = Impl::Mode::Client;
    impl_->id = 0;  // until the server welcomes us
    return true;
}

void Multiplayer::close() {
    impl_->reset();  // (disconnects everyone)
    impl_->events.clear();
}

bool Multiplayer::isActive() const { return impl_->mode != Impl::Mode::Off; }
bool Multiplayer::isServer() const { return impl_->mode != Impl::Mode::Client; }
bool Multiplayer::isConnected() const {
    return impl_->mode == Impl::Mode::Server || (impl_->mode == Impl::Mode::Client && impl_->accepted);
}
int Multiplayer::uniqueId() const { return impl_->mode == Impl::Mode::Off ? 1 : impl_->id; }
int Multiplayer::remoteSenderId() const { return impl_->sender; }

std::vector<int> Multiplayer::peers() const {
    std::vector<int> out;
    if (impl_->mode == Impl::Mode::Server) out.assign(impl_->clients.begin(), impl_->clients.end());
    if (impl_->mode == Impl::Mode::Client) out.assign(impl_->others.begin(), impl_->others.end());
    return out;
}

void Multiplayer::sendRpc(const Node* node, const std::string& method, const NetArgs& args, int target) {
    if (!node) return;
    Node* self = const_cast<Node*>(node);
    if (target != 0 && target == uniqueId()) {  // to ourselves: just call it
        json list = json::array();
        for (const NetValue& a : args) list.push_back(toJson(a));
        impl_->runRpc(self, "", method, list, uniqueId());
        return;
    }
    if (!isConnected()) return;  // not networked: nobody else to tell
    json list = json::array();
    for (const NetValue& a : args) list.push_back(toJson(a));
    json msg = {{"t", "rpc"}, {"p", pathBetween(self->root(), self)}, {"m", method}, {"a", list}};
    if (impl_->mode == Impl::Mode::Server) {
        msg["from"] = 1;
        if (target == 0) impl_->broadcast(msg, true);
        else if (impl_->clients.count(target)) impl_->send(target, msg, true);
    } else {
        msg["to"] = target;
        if (impl_->viewTime >= 0) msg["vt"] = impl_->viewTime;  // what we were seeing, for lag compensation
        impl_->send(1, msg, true);
    }
}

bool Multiplayer::isReplaying() const { return impl_->replaying; }
float Multiplayer::predictionError() const { return impl_->predError; }
int Multiplayer::ping() const { return impl_->mode == Impl::Mode::Client ? impl_->transport.roundTripMs(1) : 0; }

void Multiplayer::simulate(SceneTree& tree, float delta) {
    Impl& m = *impl_;
    if (!tree.root()) return;
    m.tickTime += delta;
    int steps = 0;
    while (m.tickTime >= kTick && steps < 8) {
        m.tickTime -= kTick;
        ++steps;
        m.tick(tree.root());
    }
    if (steps == 8) m.tickTime = 0;  // far behind (a long pause): don't try to catch up
}

void Multiplayer::lagCompensated(const Node* caller, const std::function<void()>& fn) {
    Impl& m = *impl_;
    int who = m.sender;
    if (m.mode != Impl::Mode::Server || who <= 1 || m.rpcViewTime < 0 || m.history.size() < 2 || !caller) {
        fn();  // nothing to rewind: not the server, not a client's RPC, or no history yet
        return;
    }
    // Never more than half a second back (a cheater could claim anything)
    double t = std::clamp(m.rpcViewTime, now() - 0.5, now());
    size_t i = 1;
    while (i + 1 < m.history.size() && m.history[i].time < t) ++i;
    const Impl::Frame &a = m.history[i - 1], &b = m.history[i];
    double f = b.time > a.time ? std::clamp((t - a.time) / (b.time - a.time), 0.0, 1.0) : 1.0;
    Node* root = const_cast<Node*>(caller)->root();
    // Move everyone but the shooter's own nodes back to then...
    std::vector<std::pair<Node2D*, raylib::Vector2>> moved;
    for (const Impl::Pose& later : b.poses) {
        if (root->getNode(later.path) != later.node || later.node->multiplayerAuthority() == who) continue;
        raylib::Vector2 then = later.position;
        for (const Impl::Pose& earlier : a.poses)
            if (earlier.node == later.node)
                then = earlier.position + (later.position - earlier.position) * static_cast<float>(f);
        moved.push_back({later.node, later.node->position});
        later.node->position = then;
    }
    fn();
    for (auto& [node, position] : moved) node->position = position;  // ...and back
}

namespace {
// Sends the events that have happened to every script, in order.
void dispatchEvents(Multiplayer::Impl& m, SceneTree& tree) {
    while (!m.events.empty()) {
        auto events = std::move(m.events);
        m.events.clear();
        for (auto& [kind, id] : events) {
            for (Node* n : scriptedNodes(tree.root())) {
                if (!n->script) continue;
                switch (kind) {
                case Multiplayer::Impl::Ev::PeerConnected: n->script->peerConnected(id); break;
                case Multiplayer::Impl::Ev::PeerDisconnected: n->script->peerDisconnected(id); break;
                case Multiplayer::Impl::Ev::Connected: n->script->connectedToServer(); break;
                case Multiplayer::Impl::Ev::Failed: n->script->connectionFailed(); break;
                case Multiplayer::Impl::Ev::ServerGone: n->script->serverDisconnected(); break;
                }
            }
        }
    }
}
}  // namespace

void Multiplayer::poll(SceneTree& tree) {
    Impl& m = *impl_;
    Node* root = tree.root();
    net::Event ev;
    while (m.mode != Impl::Mode::Off && m.transport.service(ev)) {
        switch (ev.kind) {
        case net::Event::Kind::Connected:
            if (m.mode == Impl::Mode::Server) {
                int pid = ev.peer;
                std::vector<int> existing(m.clients.begin(), m.clients.end());
                m.clients.insert(pid);
                m.send(pid, {{"t", "welcome"}, {"id", pid}, {"peers", existing}}, true);
                m.broadcast({{"t", "peer+"}, {"id", pid}}, true, pid);
                m.catchUp(root, pid);
                m.events.push_back({Impl::Ev::PeerConnected, pid});
            }
            break;
        case net::Event::Kind::Received: {
            json msg = json::from_cbor(ev.data, true, false);
            if (!msg.is_discarded() && msg.is_object()) m.handle(root, msg, ev.peer);
            break;
        }
        case net::Event::Kind::Disconnected:
            if (m.mode == Impl::Mode::Server) {
                if (m.clients.erase(ev.peer)) {
                    m.broadcast({{"t", "peer-"}, {"id", ev.peer}}, true);
                    m.events.push_back({Impl::Ev::PeerDisconnected, ev.peer});
                }
            } else {
                bool wasAccepted = m.accepted;
                m.reset();  // back to playing alone
                m.events.push_back({wasAccepted ? Impl::Ev::ServerGone : Impl::Ev::Failed, 0});
                print(wasAccepted ? "Disconnected from the server." : "Couldn't connect to the server.");
            }
            break;
        }
        // Events reach scripts in the order things happened (before the next message)
        dispatchEvents(m, tree);
    }
    dispatchEvents(m, tree);
    m.interpolateAll(tree.root());  // other players' nodes, smoothly, before scripts look at them
}

void Multiplayer::flush(SceneTree& tree) {
    Impl& m = *impl_;
    if (!isConnected() || !tree.root()) return;
    Node* root = tree.root();
    if (root != m.spawnRoot) {  // another scene: nothing of it was spawned yet
        m.spawners.clear();
        m.spawnRoot = root;
    }
    if (m.mode == Impl::Mode::Server) m.recordHistory(root);
    forEachNode(root, [&](Node* n) {
        if (auto* sync = dynamic_cast<MultiplayerSynchronizer*>(n)) m.updateSynchronizer(root, *sync, tree.delta());
        else if (auto* spawner = dynamic_cast<MultiplayerSpawner*>(n); spawner && m.mode == Impl::Mode::Server)
            m.updateSpawner(root, *spawner);
    });
    m.transport.flush();
}

}  // namespace s2d
