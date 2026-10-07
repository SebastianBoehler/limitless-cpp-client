#pragma once

#include <string>

namespace limitless
{
    // Optional libcurl routing for REST. The Socket.IO stub does not apply these.
    struct NetworkOptions
    {
        std::string proxy_url;
        std::string interface_name;
        long timeout_ms{20000};
        long connect_timeout_ms{10000};
    };
}
