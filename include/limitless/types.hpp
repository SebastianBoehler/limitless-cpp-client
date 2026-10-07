#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace limitless {

enum class OrderSide : uint8_t { Buy = 0, Sell = 1 };

enum class OrderType { Gtc, Fak, Fok };

enum class OutcomeToken { Yes, No };

inline const char *order_type_wire(OrderType type) {
    switch (type) {
        case OrderType::Gtc:
            return "GTC";
        case OrderType::Fak:
            return "FAK";
        case OrderType::Fok:
            return "FOK";
    }
    return "GTC";
}

struct Venue {
    std::string exchange;
    std::string adapter;
};

struct CollateralToken {
    std::string address;
    int decimals = 6;
    std::string symbol;
};

struct MarketSummary {
    std::int64_t id = 0;
    std::string slug;
    std::string title;
    std::string status;
    std::string trade_type;
    std::string market_type;
    std::string yes_token;
    std::string no_token;
    Venue venue;
    CollateralToken collateral;
    bool fee = false;
    bool is_group = false;
};

struct ActiveMarketsPage {
    std::vector<MarketSummary> data;
    double total_markets_count = 0;
};

struct ActiveMarketsQuery {
    std::optional<int> page;
    std::optional<int> limit;
    std::string sort_by;
    std::string trade_type;
    std::string automation_type;
    std::optional<bool> include_next_market;
};

struct UnsignedOrder {
    std::string salt;
    std::string maker;
    std::string signer;
    std::string taker = "0x0000000000000000000000000000000000000000";
    std::string token_id;
    std::string maker_amount;
    std::string taker_amount;
    std::string expiration = "0";
    std::string nonce = "0";
    std::string fee_rate_bps = "0";
    uint8_t side = 0;
    uint8_t signature_type = 0;
};

struct Eip712Domain {
    std::string name = "Limitless CTF Exchange";
    std::string version = "1";
    std::uint64_t chain_id = 8453;
    std::string verifying_contract;
};

struct SignedOrder {
    UnsignedOrder order;
    std::string signature;
    std::string price;
};

struct OrderRequest {
    std::string market_slug;
    OutcomeToken outcome = OutcomeToken::Yes;
    std::string token_id;
    OrderSide side = OrderSide::Buy;
    OrderType type = OrderType::Gtc;
    std::string price;
    std::string size;
    std::string maker_amount;
    std::optional<int> fee_rate_bps;
    std::optional<std::int64_t> owner_id;
    bool post_only = false;
    std::string client_order_id;
    std::optional<std::int64_t> timestamp_ms;
    std::optional<int> recv_window_ms;
    std::string stp_policy;
    bool async_submit = false;
    std::optional<std::int64_t> on_behalf_of;
    bool delegated = false;
    std::string maker;
    std::string signer;
};

struct HmacCredentials {
    std::string token_id;
    std::string secret_base64;
};

struct ScaledAmounts {
    std::string maker_amount;
    std::string taker_amount;
    std::string price;
};

}  // namespace limitless
