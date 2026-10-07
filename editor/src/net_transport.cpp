#include "net_transport.hpp"

#include <cstdint>
#include <cstdlib>
#include <map>

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
}  // namespace

struct Transport::Impl {
    ENetHost* host = nullptr;
    bool server = false;
    int nextId = 2;
    std::map<int, ENetPeer*> peers;
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
    if (!impl_->host) return false;
    ENetEvent ev;
    while (enet_host_service(impl_->host, &ev, 0) > 0) {
        switch (ev.type) {
        case ENET_EVENT_TYPE_CONNECT:
            if (impl_->server) {
                int id = impl_->nextId++;
                ev.peer->data = reinterpret_cast<void*>(static_cast<std::intptr_t>(id));
                enet_peer_timeout(ev.peer, 0, 3000, 6000);
                impl_->peers[id] = ev.peer;
            }
            out = {Event::Kind::Connected, idOf(ev.peer), {}};
            return true;
        case ENET_EVENT_TYPE_RECEIVE:
            out = {Event::Kind::Received, idOf(ev.peer),
                   std::vector<std::uint8_t>(ev.packet->data, ev.packet->data + ev.packet->dataLength)};
            enet_packet_destroy(ev.packet);
            return true;
        case ENET_EVENT_TYPE_DISCONNECT:
            out = {Event::Kind::Disconnected, idOf(ev.peer), {}};
            impl_->peers.erase(out.peer);
            return true;
        default:
            break;
        }
    }
    return false;
}

void Transport::send(int peer, const std::vector<std::uint8_t>& data, bool reliable) {
    auto it = impl_->peers.find(peer);
    if (it == impl_->peers.end()) return;
    ENetPacket* packet = enet_packet_create(data.data(), data.size(), reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    enet_peer_send(it->second, reliable ? 0 : 1, packet);
}

void Transport::flush() {
    if (impl_->host) enet_host_flush(impl_->host);
}

void Transport::close() {
    if (!impl_->host) return;
    for (auto& [id, peer] : impl_->peers) enet_peer_disconnect_now(peer, 0);
    enet_host_flush(impl_->host);
    enet_host_destroy(impl_->host);
    impl_->host = nullptr;
    impl_->peers.clear();
    impl_->nextId = 2;
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
void Transport::close() {}

#endif

}  // namespace s2d::net
