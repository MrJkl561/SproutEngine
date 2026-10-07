#include "multiplayer.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
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

    void runRpc(Node* root, const std::string& path, const std::string& method, const json& args, int from) {
        Node* node = root ? root->getNode(path) : nullptr;
        if (!node || !node->script) return;  // not in this game's scene (yet)
        NetArgs list;
        for (const json& a : args) list.push_back(fromJson(a));
        int before = sender;
        sender = from;
        if (!node->script->callRpc(method, list))
            print("RPC '" + method + "' isn't marked RPC(" + method + ") in script " + node->scriptName);
        sender = before;
    }

    // An update from `origin` (the game that owns the node) for a synchronizer
    void receiveSync(Node* root, const json& msg, int origin) {
        std::string path = msg.value("p", "");
        auto* sync = root ? dynamic_cast<MultiplayerSynchronizer*>(root->getNode(path)) : nullptr;
        Node* target = sync ? sync->getNode(sync->rootPath) : nullptr;
        if (!target || target->isMultiplayerAuthority()) return;
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
                parent->addChild(std::move(node));
            } else if (t == "despawn") {
                Node* parent = spawnParent(root, msg.value("p", ""));
                std::string name = msg.value("name", "");
                if (Node* n = parent && !name.empty() ? parent->getNode(name) : nullptr; n && n != parent)
                    n->queueFree();
            }
            return;
        }

        // The server
        if (t == "rpc") {
            int to = msg.value("to", 0);
            json out = msg;
            out.erase("to");
            out["from"] = from;
            if (to == 0) {
                runRpc(root, msg.value("p", ""), msg.value("m", ""), msg.value("a", json::array()), from);
                broadcast(out, true, from);
            } else if (to == 1) {
                runRpc(root, msg.value("p", ""), msg.value("m", ""), msg.value("a", json::array()), from);
            } else if (clients.count(to)) {
                send(to, out, true);
            }
        } else if (t == "sync") {
            // Only the node's authority may change it
            auto* sync = root ? dynamic_cast<MultiplayerSynchronizer*>(root->getNode(msg.value("p", ""))) : nullptr;
            Node* target = sync ? sync->getNode(sync->rootPath) : nullptr;
            if (!target || target->multiplayerAuthority() != from) return;
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
        if (!target || !target->isMultiplayerAuthority()) return;
        sync.sinceSent += delta;
        if (sync.sinceSent < std::max(0.0f, sync.interval)) return;
        json values = propertyListToJson(syncedProperties(sync, target));
        std::string text = values.dump();
        // Unchanged values are sent again now and then, in case one got lost.
        if (text == sync.lastSent && sync.sinceSent < 1.0) return;
        sync.sinceSent = 0;
        sync.lastSent = text;
        json msg = {{"t", "sync"}, {"p", pathBetween(root, &sync)}, {"v", values}, {"ts", now()}};
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
    impl_->mode = Impl::Mode::Server;
    impl_->id = 1;
    return true;
}

bool Multiplayer::join(const std::string& address, int port) {
    close();
    lastError.clear();
    if (!impl_->transport.connect(address, port, lastError)) return false;
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
        impl_->send(1, msg, true);
    }
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
    forEachNode(root, [&](Node* n) {
        if (auto* sync = dynamic_cast<MultiplayerSynchronizer*>(n)) m.updateSynchronizer(root, *sync, tree.delta());
        else if (auto* spawner = dynamic_cast<MultiplayerSpawner*>(n); spawner && m.mode == Impl::Mode::Server)
            m.updateSpawner(root, *spawner);
    });
    m.transport.flush();
}

}  // namespace s2d
