#include "signals.hpp"

#include <algorithm>

namespace s2d {

Node* detail::variantNode(const Variant& v) {
    auto* p = std::get_if<Node*>(&v);
    return p ? *p : nullptr;
}

std::size_t SignalHub::add(const std::string& signal, std::function<void(const SignalArgs&)> call,
                           std::weak_ptr<void> owner, bool hasOwner, bool oneShot) {
    std::size_t id = nextId++;
    connections.push_back({id, signal, std::move(call), std::move(owner), hasOwner, oneShot});
    return id;
}

void SignalHub::remove(std::size_t id) {
    connections.erase(std::remove_if(connections.begin(), connections.end(),
                                     [id](const Connection& c) { return c.id == id; }),
                      connections.end());
}

void SignalHub::emit(const std::string& signal, const SignalArgs& args) {
    // The listeners as they are now: a call may connect or disconnect others.
    std::vector<std::size_t> ids;
    for (const Connection& c : connections)
        if (c.signal == signal) ids.push_back(c.id);
    for (std::size_t id : ids) {
        auto it = std::find_if(connections.begin(), connections.end(), [id](const Connection& c) { return c.id == id; });
        if (it == connections.end()) continue;  // disconnected by an earlier call
        if (it->hasOwner && it->owner.expired()) {
            connections.erase(it);
            continue;
        }
        auto call = it->call;  // (kept while it runs, even if it disconnects itself)
        if (it->oneShot) connections.erase(it);
        call(args);
    }
}

bool SignalHub::connected(const std::string& signal) const {
    return std::any_of(connections.begin(), connections.end(), [&](const Connection& c) { return c.signal == signal; });
}

}  // namespace s2d
