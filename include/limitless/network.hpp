#pragma once

#include <string>

namespace limitless {

struct NetworkRoute {
    std::string proxy_url;
    std::string interface_name;
};

void set_default_network_route(NetworkRoute route);
NetworkRoute default_network_route();

}  // namespace limitless
