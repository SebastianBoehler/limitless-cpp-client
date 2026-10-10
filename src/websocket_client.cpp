#include "limitless/websocket_client.hpp"

#include "limitless/endpoints.hpp"

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>

#include <chrono>
#include <mutex>
#include <utility>

namespace limitless
{
    namespace
    {
        nlohmann::json subscription_payload(const MarketPriceSubscription &subscription)
        {
            nlohmann::json payload = nlohmann::json::object();
            if (!subscription.market_addresses.empty())
            {
                payload["marketAddresses"] = subscription.market_addresses;
            }
            if (!subscription.market_slugs.empty())
            {
                payload["marketSlugs"] = subscription.market_slugs;
            }
            return payload;
        }

        std::string websocket_url(const Environment &environment)
        {
            std::string base = environment.websocket_url;
            while (!base.empty() && base.back() == '/')
            {
                base.pop_back();
            }
            return base + std::string(endpoints::socket_io_handshake_path);
        }
    }

    std::string SocketIoCodec::encode_namespace_connect(std::string_view nsp)
    {
        return std::string("40") + std::string(nsp) + ",";
    }

    std::string SocketIoCodec::encode_event(std::string_view nsp,
                                            std::string_view event,
                                            const nlohmann::json *payload)
    {
        nlohmann::json packet = nlohmann::json::array();
        packet.push_back(std::string(event));
        if (payload != nullptr)
        {
            packet.push_back(*payload);
        }
        return std::string("42") + std::string(nsp) + "," + packet.dump();
    }

    SocketIoPacket SocketIoCodec::decode(std::string_view frame)
    {
        SocketIoPacket packet;
        if (frame.empty() || frame.front() < '0' || frame.front() > '9')
        {
            return packet;
        }
        const int engine_type = frame.front() - '0';
        const auto rest = frame.substr(1);
        if (engine_type == 2)
        {
            packet.kind = SocketIoPacket::Kind::Ping;
            return packet;
        }
        if (engine_type == 3)
        {
            packet.kind = SocketIoPacket::Kind::Pong;
            return packet;
        }
        if (engine_type == 0)
        {
            packet.kind = SocketIoPacket::Kind::Open;
            if (!rest.empty())
            {
                auto parsed = nlohmann::json::parse(rest, nullptr, false);
                if (!parsed.is_discarded())
                {
                    packet.data = std::move(parsed);
                }
            }
            return packet;
        }
        if (engine_type != 4 || rest.empty() || rest.front() < '0' || rest.front() > '9')
        {
            return packet;
        }
        const int socket_type = rest.front() - '0';
        std::string_view body = rest.substr(1);
        if (!body.empty() && body.front() == '/')
        {
            const auto comma = body.find(',');
            if (comma == std::string_view::npos)
            {
                packet.nsp = std::string(body);
                body = {};
            }
            else
            {
                packet.nsp = std::string(body.substr(0, comma));
                body = body.substr(comma + 1);
            }
        }
        else
        {
            packet.nsp = "/";
        }
        if (!body.empty())
        {
            auto parsed = nlohmann::json::parse(body, nullptr, false);
            if (!parsed.is_discarded())
            {
                packet.data = std::move(parsed);
            }
        }
        if (socket_type == 0)
        {
            packet.kind = SocketIoPacket::Kind::NamespaceConnect;
            return packet;
        }
        if (socket_type == 2 && packet.data.is_array() && !packet.data.empty() && packet.data[0].is_string())
        {
            packet.kind = SocketIoPacket::Kind::Event;
            SocketIoEvent event;
            event.nsp = packet.nsp;
            event.name = packet.data[0].get<std::string>();
            event.args = nlohmann::json::array();
            for (std::size_t index = 1; index < packet.data.size(); ++index)
            {
                event.args.push_back(packet.data[index]);
            }
            packet.event = std::move(event);
            return packet;
        }
        return packet;
    }

    struct MarketsSocket::Impl
    {
        Environment environment;
        std::optional<HmacCredentials> credentials;
        ix::WebSocket socket;
        mutable std::mutex mutex;
        EventCallback on_event;
        VoidCallback on_connect;
        ErrorCallback on_disconnect;
        bool namespace_connected{false};
        bool started{false};
        std::string last_error;
        std::chrono::steady_clock::time_point handshake_at{};
        std::optional<nlohmann::json> market_prices;
        std::optional<nlohmann::json> positions;
        bool order_events{false};
        bool transactions{false};

        Impl()
        {
            static std::once_flag once;
            std::call_once(once, [] { ix::initNetSystem(); });
        }

        ~Impl()
        {
            socket.stop();
        }

        bool auth_window_open() const
        {
            return std::chrono::steady_clock::now() - handshake_at < std::chrono::seconds(25);
        }

        void send_locked(const std::string &packet)
        {
            socket.send(packet);
        }

        void replay_locked()
        {
            const auto nsp = environment.websocket_namespace;
            if (market_prices)
            {
                send_locked(SocketIoCodec::encode_event(nsp, "subscribe_market_prices", &*market_prices));
            }
            if (positions && auth_window_open())
            {
                send_locked(SocketIoCodec::encode_event(nsp, "subscribe_positions", &*positions));
            }
            if (order_events && auth_window_open())
            {
                send_locked(SocketIoCodec::encode_event(nsp, "subscribe_order_events", nullptr));
            }
            if (transactions && auth_window_open())
            {
                send_locked(SocketIoCodec::encode_event(nsp, "subscribe_transactions", nullptr));
            }
        }
    };

    MarketsSocket::MarketsSocket(Environment environment)
        : impl_(std::make_unique<Impl>())
    {
        impl_->environment = std::move(environment);
        impl_->environment.validate();
    }

    MarketsSocket::~MarketsSocket()
    {
        stop();
    }

    MarketsSocket::MarketsSocket(MarketsSocket &&) noexcept = default;
    MarketsSocket &MarketsSocket::operator=(MarketsSocket &&) noexcept = default;

    void MarketsSocket::set_credentials(HmacCredentials credentials)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->credentials = std::move(credentials);
    }

    void MarketsSocket::clear_credentials()
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->credentials.reset();
    }

    void MarketsSocket::on_event(EventCallback callback)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->on_event = std::move(callback);
    }

    void MarketsSocket::on_connect(VoidCallback callback)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->on_connect = std::move(callback);
    }

    void MarketsSocket::on_disconnect(ErrorCallback callback)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->on_disconnect = std::move(callback);
    }

    void MarketsSocket::start()
    {
        stop();
        Impl *self = impl_.get();
        ix::WebSocketHttpHeaders headers;
        {
            std::lock_guard<std::mutex> lock(self->mutex);
            self->namespace_connected = false;
            self->last_error.clear();
            self->handshake_at = std::chrono::steady_clock::now();
            if (self->credentials)
            {
                const auto signed_headers = sign_websocket_handshake_now(*self->credentials);
                headers["lmts-api-key"] = signed_headers.api_key;
                headers["lmts-timestamp"] = signed_headers.timestamp;
                headers["lmts-signature"] = signed_headers.signature;
            }
        }
        self->socket.setExtraHeaders(headers);
        self->socket.setUrl(websocket_url(self->environment));
        self->socket.disableAutomaticReconnection();
        self->socket.setPingInterval(0);
        self->socket.setOnMessageCallback([self](const ix::WebSocketMessagePtr &message) {
            if (message->type == ix::WebSocketMessageType::Message)
            {
                const auto packet = SocketIoCodec::decode(message->str);
                if (packet.kind == SocketIoPacket::Kind::Ping)
                {
                    self->socket.send("3");
                    return;
                }
                if (packet.kind == SocketIoPacket::Kind::Open)
                {
                    self->socket.send(SocketIoCodec::encode_namespace_connect(self->environment.websocket_namespace));
                    return;
                }
                EventCallback event_callback;
                VoidCallback connect_callback;
                {
                    std::lock_guard<std::mutex> callback_lock(self->mutex);
                    if (packet.kind == SocketIoPacket::Kind::NamespaceConnect &&
                        packet.nsp == self->environment.websocket_namespace)
                    {
                        self->namespace_connected = true;
                        self->replay_locked();
                        connect_callback = self->on_connect;
                    }
                    else if (packet.kind == SocketIoPacket::Kind::Event && packet.event)
                    {
                        event_callback = self->on_event;
                    }
                }
                if (connect_callback)
                {
                    connect_callback();
                }
                if (event_callback && packet.kind == SocketIoPacket::Kind::Event)
                {
                    event_callback(*packet.event);
                }
                return;
            }
            if (message->type == ix::WebSocketMessageType::Close || message->type == ix::WebSocketMessageType::Error)
            {
                ErrorCallback callback;
                std::string reason = message->errorInfo.reason;
                if (reason.empty())
                {
                    reason = "socket closed";
                }
                {
                    std::lock_guard<std::mutex> callback_lock(self->mutex);
                    self->namespace_connected = false;
                    self->last_error = reason;
                    callback = self->on_disconnect;
                }
                if (callback)
                {
                    callback(reason);
                }
            }
        });
        self->socket.start();
        std::lock_guard<std::mutex> lock(self->mutex);
        self->started = true;
    }

    void MarketsSocket::stop()
    {
        if (!impl_)
        {
            return;
        }
        impl_->socket.stop();
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->started = false;
        impl_->namespace_connected = false;
    }

    bool MarketsSocket::connected() const
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->namespace_connected;
    }

    bool MarketsSocket::subscribe_market_prices(const MarketPriceSubscription &subscription)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->market_prices = subscription_payload(subscription);
        if (impl_->namespace_connected)
        {
            impl_->send_locked(SocketIoCodec::encode_event(impl_->environment.websocket_namespace,
                                                           "subscribe_market_prices",
                                                           &*impl_->market_prices));
        }
        return true;
    }

    bool MarketsSocket::subscribe_positions(const MarketPriceSubscription &subscription)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->credentials)
        {
            impl_->last_error = "subscribe_positions requires HMAC credentials";
            return false;
        }
        if (impl_->namespace_connected && !impl_->auth_window_open())
        {
            impl_->last_error = "authenticated subscribe is outside the 30s handshake window; call start() again";
            return false;
        }
        impl_->positions = subscription_payload(subscription);
        if (impl_->namespace_connected)
        {
            impl_->send_locked(SocketIoCodec::encode_event(impl_->environment.websocket_namespace,
                                                           "subscribe_positions",
                                                           &*impl_->positions));
        }
        return true;
    }

    bool MarketsSocket::subscribe_order_events()
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->credentials)
        {
            impl_->last_error = "subscribe_order_events requires HMAC credentials";
            return false;
        }
        if (impl_->namespace_connected && !impl_->auth_window_open())
        {
            impl_->last_error = "authenticated subscribe is outside the 30s handshake window; call start() again";
            return false;
        }
        impl_->order_events = true;
        if (impl_->namespace_connected)
        {
            impl_->send_locked(SocketIoCodec::encode_event(impl_->environment.websocket_namespace,
                                                           "subscribe_order_events",
                                                           nullptr));
        }
        return true;
    }

    bool MarketsSocket::subscribe_transactions()
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->credentials)
        {
            impl_->last_error = "subscribe_transactions requires HMAC credentials";
            return false;
        }
        if (impl_->namespace_connected && !impl_->auth_window_open())
        {
            impl_->last_error = "authenticated subscribe is outside the 30s handshake window; call start() again";
            return false;
        }
        impl_->transactions = true;
        if (impl_->namespace_connected)
        {
            impl_->send_locked(
                SocketIoCodec::encode_event(impl_->environment.websocket_namespace, "subscribe_transactions", nullptr));
        }
        return true;
    }

    std::string MarketsSocket::last_error() const
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->last_error;
    }
}
