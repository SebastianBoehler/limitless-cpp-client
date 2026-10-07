#pragma once

#include "limitless/websocket_client.hpp"

namespace limitless
{
    // Authenticated user channels on the same /markets Socket.IO namespace.
    // subscribe_positions, subscribe_order_events, and subscribe_transactions
    // are re-checked against the handshake timestamp (30 seconds).
    class UserStream
    {
    public:
        UserStream(Environment environment, HmacCredentials credentials);
        ~UserStream();

        UserStream(UserStream &&) noexcept;
        UserStream &operator=(UserStream &&) noexcept;
        UserStream(const UserStream &) = delete;
        UserStream &operator=(const UserStream &) = delete;

        void on_event(MarketsSocket::EventCallback callback);
        void on_connect(MarketsSocket::VoidCallback callback);
        void on_disconnect(MarketsSocket::ErrorCallback callback);

        void start();
        void stop();

        bool subscribe_positions(const MarketPriceSubscription &subscription);
        bool subscribe_order_events();
        bool subscribe_transactions();

        std::string last_error() const;

    private:
        MarketsSocket socket_;
    };
}
