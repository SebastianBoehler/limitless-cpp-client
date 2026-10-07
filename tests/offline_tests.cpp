#include "limitless/decimal_math.hpp"
#include "limitless/environment.hpp"
#include "limitless/hmac_auth.hpp"
#include "limitless/json_util.hpp"
#include "limitless/order_signer.hpp"
#include "limitless/orderbook.hpp"
#include "limitless/socket_io.hpp"
#include "limitless/version.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int g_failed = 0;

void expect(bool cond, const std::string &label) {
    if (!cond) {
        std::cerr << "FAIL " << label << "\n";
        ++g_failed;
    }
}

std::string hex32(const std::array<std::uint8_t, 32> &bytes) {
    return limitless::to_hex(bytes.data(), bytes.size(), false);
}

}  // namespace

int main() {
    expect(std::string(limitless::version_string) == "0.1.0", "version");
    expect(limitless::Environment::production().chain_id == 8453, "chain");
    expect(limitless::Environment::production().websocket_connect_url() ==
               "wss://ws.limitless.exchange/socket.io/?EIO=4&transport=websocket",
           "ws url");

    expect(hex32(limitless::keccak256(std::string_view{})) ==
               "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470",
           "keccak empty");
    expect(hex32(limitless::keccak256(std::string_view{"abc"})) ==
               "4e03657aea45a94fc7d47ba826c8d667c0d1e6e33a64a036ec44f58fa12d6c45",
           "keccak abc");

    limitless::OrderSigner signer(
        "0x4c0883a69102937d6231471b5dbb6204fe5129617082792ae468d01a3f362318");
    expect(signer.address() == "0x2c7536E3605D9C16a7a3D7b1898e529396a65c23", "address");

    limitless::Eip712Domain domain;
    domain.verifying_contract = "0x05c748E2f4DcDe0ec9Fa8DDc40DE6b867f923fa5";
    limitless::UnsignedOrder order;
    order.salt = "1234567890";
    order.maker = signer.address();
    order.signer = signer.address();
    order.token_id = "61516833861957014596392994015394669033519057856611921644681759117769431583794";
    order.maker_amount = "5000000";
    order.taker_amount = "10000000";
    auto digest = signer.order_digest(domain, order);
    expect(static_cast<bool>(digest), "digest ok");
    if (digest) {
        expect(hex32(digest.value()) == "e7879802bb58556fceadba8f63a77f36a733cc127cd371c3a301f13db85f82fb",
               "digest");
    }
    auto signed_order = signer.sign_order(domain, order, "0.5");
    expect(static_cast<bool>(signed_order), "sign");
    if (signed_order) {
        expect(signed_order.value().signature ==
                   "0xe610c0e835cf2c845bf7b28d8f09db9d1da0e35bda95eff71983c6f38ea37bd93218ea4e6b1b5dbf494"
                   "fa62e0ef0d41df3475dc07eca7beeb3808b09ec771f891c",
               "signature");
    }

    limitless::OrderSigner signer2("11");
    expect(signer2.address().empty(), "short key");
    limitless::OrderSigner signer3(std::string(64, '1'));
    expect(signer3.address() == "0x19E7E376E7C213B7E7e7e46cc70A5dD086DAff2A", "address2");
    limitless::UnsignedOrder sell = order;
    sell.salt = "1";
    sell.maker = signer3.address();
    sell.signer = signer3.address();
    sell.maker_amount = "10000000";
    sell.taker_amount = "5000000";
    sell.fee_rate_bps = "200";
    sell.side = 1;
    auto signed_sell = signer3.sign_order(domain, sell);
    expect(static_cast<bool>(signed_sell), "sign sell");
    if (signed_sell) {
        expect(signed_sell.value().signature ==
                   "0xbf3d8e455bc0cfa77ee92ce7a6ac80797aac7d9bf0e635ca3d421a366127c8b265deee05b93dff5a38"
                   "94e4b1b6e6648680fe74e781b6c283cc1c251f67c2f6851b",
               "signature2");
    }

    limitless::HmacCredentials creds{"token-id", "c3VwZXJzZWNyZXQhIQ=="};
    const auto headers = limitless::sign_request(creds, "POST", "/orders?onBehalfOf=42", "{\"a\":1}",
                                                 "2026-10-07T12:00:00.000Z");
    expect(headers.signature == "gzbiBVOKFBG0fYpi0LjZQ7aTzUgPQ/h0hMuv1CQ9Sa0=", "hmac");
    expect(headers.headers.size() == 3, "hmac headers");
    const auto ws = limitless::sign_websocket_handshake(
        creds, "/socket.io/?EIO=4&transport=websocket", "2026-10-07T12:00:00.000Z");
    expect(!ws.signature.empty() && ws.headers[0].first == "lmts-api-key", "ws hmac");

    limitless::ScaledAmounts amounts;
    std::string error;
    expect(limitless::gtc_amounts(limitless::OrderSide::Buy, "0.50", "10", amounts, error), "gtc");
    expect(amounts.maker_amount == "5000000" && amounts.taker_amount == "10000000" && amounts.price == "0.5",
           "gtc values");
    expect(limitless::gtc_amounts(limitless::OrderSide::Sell, "0.50", "10", amounts, error), "gtc sell");
    expect(amounts.maker_amount == "10000000" && amounts.taker_amount == "5000000", "gtc sell values");
    expect(!limitless::gtc_amounts(limitless::OrderSide::Buy, "0.1234", "10", amounts, error), "price scale");
    expect(limitless::fok_amounts(limitless::OrderSide::Buy, "5", amounts, error), "fok");
    expect(amounts.maker_amount == "5000000" && amounts.taker_amount == "1" && amounts.price.empty(),
           "fok values");

    limitless::Orderbook yes;
    yes.bids.push_back({0.53, 100000000, "BUY"});
    yes.asks.push_back({0.55, 80000000, "SELL"});
    yes.midpoint = 0.54;
    const auto no = limitless::derive_no_book(yes);
    expect(!no.bids.empty() && no.bids.front().price > 0.44 && no.bids.front().price < 0.46, "no bid");
    expect(no.bids.front().side == "BUY" && no.asks.front().side == "SELL", "no sides");

    nlohmann::json frame = {{"marketSlug", "btc"},
                            {"version", 2},
                            {"orderbook", {{"bids", nlohmann::json::array()}, {"asks", nlohmann::json::array()},
                                           {"midpoint", 0.5}}}};
    limitless::OrderbookManager manager;
    expect(manager.apply_update(frame), "apply");
    frame["version"] = 1;
    expect(!manager.apply_update(frame), "stale version");
    frame["version"] = 0;
    expect(!manager.apply_update(frame), "fallback after live");

    const auto packet = limitless::socketio_event_packet(
        "/markets", "subscribe_market_prices", nlohmann::json{{"marketSlugs", nlohmann::json::array({"btc"})}});
    expect(packet == "42/markets,[\"subscribe_market_prices\",{\"marketSlugs\":[\"btc\"]}]", "sio event");
    const auto parsed = limitless::parse_socket_packet(packet);
    expect(parsed.kind == limitless::SocketPacketKind::Event && parsed.event == "subscribe_market_prices",
           "sio parse");
    expect(limitless::parse_socket_packet("2").kind == limitless::SocketPacketKind::Ping, "ping");
    expect(limitless::socketio_connect_packet("/markets") == "40/markets,", "connect packet");

    const auto quoted = limitless::parse_json("{\"tokenId\":615168338619570145963929940153946690335190578566}");
    expect(quoted["tokenId"].is_string(), "big int preserved");

    if (g_failed) {
        std::cerr << g_failed << " failed\n";
        return 1;
    }
    std::cout << "ok\n";
    return 0;
}
