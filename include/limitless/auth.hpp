#pragma once

#include "limitless/endpoints.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace limitless
{
    struct HmacCredentials
    {
        std::string token_id;
        // Base64 secret returned once when the token is derived. Decoded before HMAC.
        std::string secret_base64;
    };

    struct HmacHeaders
    {
        std::string api_key;
        std::string timestamp;
        std::string signature;

        std::vector<std::pair<std::string, std::string>> fields() const;
    };

    // Canonical message: "{timestamp}\n{METHOD}\n{path+query}\n{body}"
    // GET and empty bodies use an empty body component. The path includes the query string.
    std::string hmac_canonical_message(std::string_view timestamp,
                                       std::string_view method,
                                       std::string_view path_and_query,
                                       std::string_view body);

    HmacHeaders sign_request(const HmacCredentials &credentials,
                             std::string_view method,
                             std::string_view path_and_query,
                             std::string_view body,
                             std::string_view timestamp);

    HmacHeaders sign_request_now(const HmacCredentials &credentials,
                                 std::string_view method,
                                 std::string_view path_and_query,
                                 std::string_view body);

    // Fixed handshake message from the WebSocket overview.
    HmacHeaders sign_websocket_handshake(const HmacCredentials &credentials,
                                         std::string_view timestamp);
    HmacHeaders sign_websocket_handshake_now(const HmacCredentials &credentials);

    std::string iso8601_utc_millis_now();
    std::string iso8601_utc_millis(std::int64_t unix_millis);
}
