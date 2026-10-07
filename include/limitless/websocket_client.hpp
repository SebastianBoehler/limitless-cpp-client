#pragma once

#include "limitless/environment.hpp"
#include "limitless/hmac_auth.hpp"
#include "limitless/http_client.hpp"
#include "limitless/sdk_error.hpp"
#include "limitless/socket_io.hpp"

#include <nlohmann/json.hpp>

#include <functional>
#include <string>
#include <vector>

namespace limitless {

struct WebSocketOptions {
    int connect_timeout_ms = 10000;
    int recv_timeout_ms = 1000;
    std::string proxy_url;
    std::string interface_name;
    std::string user_agent = "limitless-cpp-client/0.1.0";
};

// Socket.IO client for wss://ws.limitless.exchange namespace /markets.
// Public market data needs no HMAC. Authenticated events
// (subscribe_positions, subscribe_order_events, subscribe_transactions)
// sign the Engine.IO handshake and must be emitted within 30 seconds of
// that timestamp; reconnect() mints a fresh signature.
class WebSocketClient {
public:
    using EventHandler = std::function<void(const SocketPacket &)>;

    explicit WebSocketClient(Environment env = Environment::production());
    ~WebSocketClient();

    WebSocketClient(const WebSocketClient &) = delete;
    WebSocketClient &operator=(const WebSocketClient &) = delete;

    void configure(WebSocketOptions options);
    void set_credentials(HmacCredentials credentials);
    void on_event(EventHandler handler);

    void subscribe_market_prices(std::vector<std::string> market_slugs,
                                 std::vector<std::string> market_addresses = {});
    void subscribe_authenticated(const std::string &event, nlohmann::json payload);

    Result<bool> connect();
    Result<bool> reconnect();
    Result<int> poll(int timeout_ms = 1000);
    void close();
    bool connected() const { return connected_; }

private:
    Environment env_;
    WebSocketOptions options_;
    HmacCredentials credentials_;
    bool have_credentials_ = false;
    EventHandler handler_;
    void *easy_ = nullptr;
    bool connected_ = false;
    std::vector<std::string> market_slugs_;
    std::vector<std::string> market_addresses_;
    std::vector<std::pair<std::string, nlohmann::json>> auth_subs_;

    Result<bool> open_socket();
    Result<bool> send_text(const std::string &payload);
    Result<std::string> recv_text(int timeout_ms);
    Result<bool> emit_subscriptions();
    void reset_easy();
};

}  // namespace limitless
