#include "limitless/network.hpp"

namespace limitless {
namespace {
NetworkRoute g_route;
}

void set_default_network_route(NetworkRoute route) { g_route = std::move(route); }

NetworkRoute default_network_route() { return g_route; }

}  // namespace limitless
