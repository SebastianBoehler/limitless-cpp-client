#include "check.hpp"
#include "limitless/auth.hpp"

int main()
{
    const limitless::HmacCredentials credentials{"token-id", "bGltaXRsZXNzLXRlc3Qtc2VjcmV0"};
    const auto headers = limitless::sign_request(credentials,
                                                 "post",
                                                 "/orders",
                                                 "{\"a\":1}",
                                                 "2026-10-07T12:00:00.000Z");
    CHECK(headers.api_key == "token-id");
    CHECK(headers.timestamp == "2026-10-07T12:00:00.000Z");
    CHECK(headers.signature == "1y+oSCbCAP7Fx5YKTw4GBFuh4Bp7dsHh0WZ3iz6HinY=");

    const auto listed = limitless::sign_request(credentials,
                                                "GET",
                                                "/markets/active?limit=1",
                                                "",
                                                "2026-10-07T12:00:00.000Z");
    CHECK(listed.signature == "iT/uj38DoNznHAzIA8IqgVYwa9vW2RR4GS6eo1HfkCU=");

    const auto handshake = limitless::sign_websocket_handshake(credentials, "2026-10-07T12:00:00.000Z");
    CHECK(handshake.signature == "h79kqnadD9K6cc7bGWN11wxChhFrGznf/dM00qWWG/E=");
    CHECK(limitless::endpoints::socket_io_handshake_path == "/socket.io/?EIO=4&transport=websocket");

    const auto message = limitless::hmac_canonical_message("2026-10-07T12:00:00.000Z", "get", "/orders", "");
    CHECK(message == "2026-10-07T12:00:00.000Z\nGET\n/orders\n");
    CHECK(limitless::iso8601_utc_millis(1'700'000'000'000).size() == 24);
    RETURN_TEST();
}
