#pragma once

#include "limitless/types.hpp"

#include <string>
#include <utility>
#include <vector>

namespace limitless {

struct HmacHeaders {
    std::string timestamp;
    std::string signature;
    std::vector<std::pair<std::string, std::string>> headers;
};

std::string iso8601_millis_now();

// Canonical message: "{timestamp}\n{METHOD}\n{path+query}\n{body}"
// Secret is base64-decoded before HMAC-SHA256. Signature is base64.
HmacHeaders sign_request(const HmacCredentials &creds, std::string_view method,
                         std::string_view path_and_query, std::string_view body,
                         std::string_view timestamp = {});

// Handshake message from the WebSocket overview:
// "{timestamp}\nGET\n/socket.io/?EIO=4&transport=websocket\n"
HmacHeaders sign_websocket_handshake(const HmacCredentials &creds,
                                     std::string_view socketio_path,
                                     std::string_view timestamp = {});

}  // namespace limitless
