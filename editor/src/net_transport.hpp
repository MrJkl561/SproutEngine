// The network connection under multiplayer.cpp: ENet (UDP) wrapped in a few
// calls. Kept apart because ENet brings in <winsock2.h> / <windows.h> on
// Windows, which can't be in the same file as raylib.h.
//
// Peers are numbered like Godot's: on the server, each client gets 2, 3, ...
// when it connects; on a client, the server is 1.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace s2d::net {

struct Event {
    enum class Kind { Connected, Received, Disconnected } kind = Kind::Received;
    int peer = 0;
    std::vector<std::uint8_t> data;  // Received: the message
};

class Transport {
public:
    Transport();
    ~Transport();
    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;

    static bool available();  // false in web games (no UDP in browsers)

    bool host(int port, int maxClients, std::string& error);
    bool connect(const std::string& address, int port, std::string& error);  // the server will be peer 1
    bool isOpen() const;

    bool service(Event& out);  // the next thing that happened; false when there's nothing
    // reliable: arrives, in order (channel 0). Otherwise it may be lost, and
    // older ones are dropped if a newer one arrived first (channel 1).
    void send(int peer, const std::vector<std::uint8_t>& data, bool reliable);
    void flush();
    void close();  // disconnects everyone at once

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace s2d::net
