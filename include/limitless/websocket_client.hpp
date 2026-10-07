#pragma once

#include "limitless/auth.hpp"
#include "limitless/environment.hpp"

#include <nlohmann/json.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace limitless
{
    struct SocketIoEvent
    {
        std::string nsp;
        std::string name;
        nlohmann::json args{nlohmann::json::array()};
    };

    struct SocketIoPacket
    {
        enum class Kind
        {
            Open,
            Ping,
            Pong,
            NamespaceConnect,
            Event,
            Other
        };

        Kind kind{Kind::Other};
        std::string nsp;
        nlohmann::json data;
        std::optional<SocketIoEvent> event;
    };

    // Engine.IO v4 / Socket.IO packet codec for the /markets namespace.
    // This is not a general Socket.IO client. See docs/websocket.md.
    struct SocketIoCodec
    {
        static std::string encode_namespace_connect(std::string_view nsp);
        static std::string encode_event(std::string_view nsp,
                                        std::string_view event,
                                        const nlohmann::json *payload);
        static SocketIoPacket decode(std::string_view frame);
    };

    struct MarketPriceSubscription
    {
        std::vector<std::string> market_addresses;
        std::vector<std::string> market_slugs;
    };

    // Minimal Socket.IO connection to wss://ws.limitless.exchange namespace /markets.
    // WebSocket transport only. The server sends Engine.IO ping packets; this client
    // answers with pong and does not send its own WebSocket PING frames.
    class MarketsSocket
    {
    public:
        using EventCallback = std::function<void(const SocketIoEvent &)>;
        using VoidCallback = std::function<void()>;
        using ErrorCallback = std::function<void(const std::string &)>;

        explicit MarketsSocket(Environment environment = Environment::base());
        ~MarketsSocket();

        MarketsSocket(MarketsSocket &&) noexcept;
        MarketsSocket &operator=(MarketsSocket &&) noexcept;
        MarketsSocket(const MarketsSocket &) = delete;
        MarketsSocket &operator=(const MarketsSocket &) = delete;

        void set_credentials(HmacCredentials credentials);
        void clear_credentials();

        void on_event(EventCallback callback);
        void on_connect(VoidCallback callback);
        void on_disconnect(ErrorCallback callback);

        void start();
        void stop();
        bool connected() const;

        // Public. Replaces the previous subscribe_market_prices set on this connection.
        bool subscribe_market_prices(const MarketPriceSubscription &subscription);

        // Authenticated. Must be emitted within 30s of the handshake timestamp.
        // A stale connection returns false and does not send.
        bool subscribe_positions(const MarketPriceSubscription &subscription);
        bool subscribe_order_events();
        bool subscribe_transactions();

        std::string last_error() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
