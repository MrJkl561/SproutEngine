#include "net_transport.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <map>
#include <random>

#if !defined(PLATFORM_WEB) && !defined(SPROUT_NO_NETWORK)
#define SPROUT_HAS_ENET 1
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif
#include <enet/enet.h>
#endif

namespace s2d::net {

#if SPROUT_HAS_ENET

namespace {
bool startEnet() {
    static bool ok = [] {
        if (enet_initialize() != 0) return false;
        std::atexit(enet_deinitialize);
        return true;
    }();
    return ok;
}
int idOf(ENetPeer* peer) { return static_cast<int>(reinterpret_cast<std::intptr_t>(peer->data)); }

double seconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
}  // namespace

struct Transport::Impl {
    ENetHost* host = nullptr;
    bool server = false;
    int nextId = 2;
    std::map<int, ENetPeer*> peers;

    // Simulated lag (simulateLag): messages wait in these, oldest due first
    double halfLag = 0, jitter = 0, loss = 0;
    struct Out {
        double due;
        int peer;
        std::vector<std::uint8_t> data;
        bool reliable;
    };
    struct In {
        double due;
        Event event;
    };
    std::deque<Out> outgoing;
    std::deque<In> incoming;
    double lastOut = 0, lastIn = 0;  // reliable things keep their order
    std::mt19937 random{12345};

    bool lagging() const { return halfLag > 0 || jitter > 0 || loss > 0; }
    double delay(bool reliable, double& last) {
        std::uniform_real_distribution<double> j(-jitter, jitter);
        double due = seconds() + std::max(0.0, halfLag + j(random));
        if (reliable) due = last = std::max(due, last);
        return due;
    }
    bool lose() {
        std::uniform_real_distribution<double> r(0, 100);
        return loss > 0 && r(random) < loss;
    }
    void sendNow(int peer, const std::vector<std::uint8_t>& data, bool reliable) {
        auto it = peers.find(peer);
        if (it == peers.end()) return;
        ENetPacket* packet = enet_packet_create(data.data(), data.size(), reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
        enet_peer_send(it->second, reliable ? 0 : 1, packet);
    }
    void releaseOutgoing() {
        double t = seconds();
        while (!outgoing.empty() && outgoing.front().due <= t) {
            sendNow(outgoing.front().peer, outgoing.front().data, outgoing.front().reliable);
            outgoing.pop_front();
        }
    }
};

Transport::Transport() : impl_(std::make_unique<Impl>()) {}
Transport::~Transport() { close(); }
bool Transport::available() { return true; }
bool Transport::isOpen() const { return impl_->host != nullptr; }

bool Transport::host(int port, int maxClients, std::string& error) {
    close();
    if (!startEnet()) {
        error = "Couldn't start networking (ENet).";
        return false;
    }
    ENetAddress address{};
    address.host = ENET_HOST_ANY;
    address.port = static_cast<enet_uint16>(port);
    impl_->host = enet_host_create(&address, static_cast<size_t>(maxClients < 1 ? 1 : maxClients), 2, 0, 0);
    if (!impl_->host) {
        error = "Couldn't host on port " + std::to_string(port) + " (is another game using it?)";
        return false;
    }
    impl_->server = true;
    return true;
}

bool Transport::connect(const std::string& address, int port, std::string& error) {
    close();
    if (!startEnet()) {
        error = "Couldn't start networking (ENet).";
        return false;
    }
    impl_->host = enet_host_create(nullptr, 1, 2, 0, 0);
    ENetAddress where{};
    if (!impl_->host || enet_address_set_host(&where, address.c_str()) != 0) {
        error = "Couldn't find the computer '" + address + "'";
        close();
        return false;
    }
    where.port = static_cast<enet_uint16>(port);
    ENetPeer* peer = enet_host_connect(impl_->host, &where, 2, 0);
    if (!peer) {
        error = "Couldn't start connecting to " + address;
        close();
        return false;
    }
    enet_peer_timeout(peer, 0, 3000, 6000);  // give up on a missing server after a few seconds
    peer->data = reinterpret_cast<void*>(static_cast<std::intptr_t>(1));
    impl_->peers[1] = peer;
    impl_->server = false;
    return true;
}

bool Transport::service(Event& out) {
    Impl& m = *impl_;
    if (!m.host) return false;
    m.releaseOutgoing();
    ENetEvent ev;
    while (enet_host_service(m.host, &ev, 0) > 0) {
        Event got;
        bool reliable = true;
        switch (ev.type) {
        case ENET_EVENT_TYPE_CONNECT:
            if (m.server) {
                int id = m.nextId++;
                ev.peer->data = reinterpret_cast<void*>(static_cast<std::intptr_t>(id));
                enet_peer_timeout(ev.peer, 0, 3000, 6000);
                m.peers[id] = ev.peer;
            }
            got = {Event::Kind::Connected, idOf(ev.peer), {}};
            break;
        case ENET_EVENT_TYPE_RECEIVE:
            reliable = (ev.packet->flags & ENET_PACKET_FLAG_RELIABLE) != 0;
            got = {Event::Kind::Received, idOf(ev.peer),
                   std::vector<std::uint8_t>(ev.packet->data, ev.packet->data + ev.packet->dataLength)};
            enet_packet_destroy(ev.packet);
            break;
        case ENET_EVENT_TYPE_DISCONNECT:
            got = {Event::Kind::Disconnected, idOf(ev.peer), {}};
            m.peers.erase(got.peer);
            break;
        default:
            continue;
        }
        if (!m.lagging()) {
            out = std::move(got);
            return true;
        }
        if (!reliable && m.lose()) continue;
        Impl::In in{m.delay(reliable, m.lastIn), std::move(got)};
        auto at = std::upper_bound(m.incoming.begin(), m.incoming.end(), in.due,
                                   [](double due, const Impl::In& x) { return due < x.due; });
        m.incoming.insert(at, std::move(in));
    }
    if (!m.incoming.empty() && m.incoming.front().due <= seconds()) {
        out = std::move(m.incoming.front().event);
        m.incoming.pop_front();
        return true;
    }
    return false;
}

void Transport::simulateLag(int lagMs, int jitterMs, float lossPercent) {
    impl_->halfLag = std::max(0, lagMs) / 2000.0;
    impl_->jitter = std::max(0, jitterMs) / 1000.0;
    impl_->loss = std::clamp(lossPercent, 0.0f, 100.0f);
}

void Transport::send(int peer, const std::vector<std::uint8_t>& data, bool reliable) {
    Impl& m = *impl_;
    if (!m.lagging()) return m.sendNow(peer, data, reliable);
    if (!reliable && m.lose()) return;
    Impl::Out out{m.delay(reliable, m.lastOut), peer, data, reliable};
    auto at = std::upper_bound(m.outgoing.begin(), m.outgoing.end(), out.due,
                               [](double due, const Impl::Out& x) { return due < x.due; });
    m.outgoing.insert(at, std::move(out));
}

void Transport::flush() {
    if (!impl_->host) return;
    impl_->releaseOutgoing();
    enet_host_flush(impl_->host);
}

int Transport::roundTripMs(int peer) const {
    auto it = impl_->peers.find(peer);
    if (it == impl_->peers.end()) return 0;
    // (ENet times its own replies, which don't wait in simulateLag's queues: add that delay)
    return static_cast<int>(it->second->roundTripTime) + static_cast<int>(impl_->halfLag * 2000);
}

void Transport::close() {
    if (!impl_->host) return;
    for (auto& [id, peer] : impl_->peers) enet_peer_disconnect_now(peer, 0);
    enet_host_flush(impl_->host);
    enet_host_destroy(impl_->host);
    impl_->host = nullptr;
    impl_->peers.clear();
    impl_->nextId = 2;
    impl_->outgoing.clear();
    impl_->incoming.clear();
}

#else  // no networking (web games)

struct Transport::Impl {};
Transport::Transport() : impl_(std::make_unique<Impl>()) {}
Transport::~Transport() = default;
bool Transport::available() { return false; }
bool Transport::isOpen() const { return false; }
bool Transport::host(int, int, std::string& error) {
    error = "Multiplayer isn't available in web games (browsers can't use UDP).";
    return false;
}
bool Transport::connect(const std::string&, int, std::string& error) {
    error = "Multiplayer isn't available in web games (browsers can't use UDP).";
    return false;
}
bool Transport::service(Event&) { return false; }
void Transport::send(int, const std::vector<std::uint8_t>&, bool) {}
void Transport::flush() {}
int Transport::roundTripMs(int) const { return 0; }
void Transport::simulateLag(int, int, float) {}
void Transport::close() {}

#endif

}  // namespace s2d::net
