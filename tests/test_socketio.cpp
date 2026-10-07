#include "check.hpp"
#include "limitless/websocket_client.hpp"

#include <nlohmann/json.hpp>

int main()
{
    limitless::MarketPriceSubscription subscription;
    subscription.market_slugs = {"btc-100k-weekly"};
    nlohmann::json payload = {{"marketSlugs", subscription.market_slugs}};
    const auto encoded = limitless::SocketIoCodec::encode_event("/markets", "subscribe_market_prices", &payload);
    CHECK(encoded == "42/markets,[\"subscribe_market_prices\",{\"marketSlugs\":[\"btc-100k-weekly\"]}]");

    const auto decoded = limitless::SocketIoCodec::decode(encoded);
    CHECK(decoded.kind == limitless::SocketIoPacket::Kind::Event);
    CHECK(decoded.event.has_value());
    CHECK(decoded.event->name == "subscribe_market_prices");
    CHECK(decoded.event->args[0]["marketSlugs"][0] == "btc-100k-weekly");

    const auto bare = limitless::SocketIoCodec::encode_event("/markets", "subscribe_order_events", nullptr);
    CHECK(bare == "42/markets,[\"subscribe_order_events\"]");
    CHECK(limitless::SocketIoCodec::encode_namespace_connect("/markets") == "40/markets,");

    const auto ping = limitless::SocketIoCodec::decode("2");
    CHECK(ping.kind == limitless::SocketIoPacket::Kind::Ping);
    const auto opened = limitless::SocketIoCodec::decode("0{\"sid\":\"abc\",\"pingInterval\":25000}");
    CHECK(opened.kind == limitless::SocketIoPacket::Kind::Open);
    CHECK(opened.data["sid"] == "abc");

    const auto frame = limitless::SocketIoCodec::decode(
        "42/markets,[\"orderbookUpdate\",{\"marketSlug\":\"btc\",\"version\":3}]");
    CHECK(frame.event->name == "orderbookUpdate");
    CHECK(frame.event->args[0]["version"] == 3);

    limitless::MarketsSocket socket(limitless::Environment::base());
    CHECK(!socket.subscribe_order_events());
    CHECK(socket.last_error().find("HMAC") != std::string::npos);
    CHECK(socket.subscribe_market_prices(subscription));
    RETURN_TEST();
}
