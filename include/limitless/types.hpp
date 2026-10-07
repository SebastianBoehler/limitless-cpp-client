#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace limitless
{
    inline constexpr int kBaseChainId = 8453;
    inline constexpr int kCollateralDecimals = 6;
    inline constexpr int kPriceDecimals = 3;
    inline constexpr char kZeroAddress[] = "0x0000000000000000000000000000000000000000";

    // EIP-712 domain published in the order-signing guide.
    inline constexpr char kExchangeDomainName[] = "Limitless CTF Exchange";
    inline constexpr char kExchangeDomainVersion[] = "1";

    enum class Side
    {
        Buy = 0,
        Sell = 1
    };

    enum class OrderType
    {
        Gtc,
        Fak,
        Fok
    };

    enum class AllowanceType
    {
        Clob,
        NegRisk
    };

    struct Venue
    {
        std::string exchange;
        std::optional<std::string> adapter;
    };

    struct TokenPair
    {
        std::string yes;
        std::string no;
    };

    // Fields used to trade. `raw` keeps the rest of the market payload.
    struct MarketSummary
    {
        std::int64_t id{0};
        std::string slug;
        std::string title;
        std::string status;
        std::string trade_type;
        std::string market_type;
        std::optional<TokenPair> tokens;
        std::optional<Venue> venue;
        bool fee{false};
        nlohmann::json raw;
    };

    struct ActiveMarketsPage
    {
        std::vector<MarketSummary> markets;
        std::optional<std::int64_t> total_markets_count;
        nlohmann::json raw;
    };

    struct Profile
    {
        std::int64_t id{0};
        std::string account;
        std::string trade_wallet_option;
        int fee_rate_bps{0};
        bool has_fee_rate{false};
        nlohmann::json raw;
    };

    struct ActiveMarketsQuery
    {
        std::optional<int> page;
        std::optional<int> limit;
        std::optional<std::string> trade_type;
        std::optional<std::string> automation_type;
        std::optional<bool> include_next_market;
    };

    struct SearchMarketsQuery
    {
        std::string query;
        std::optional<int> limit;
        std::optional<int> page;
        std::optional<double> similarity_threshold;
        std::optional<bool> include_next_market;
    };

    struct UserOrdersQuery
    {
        std::vector<std::string> statuses;
        std::optional<int> limit;
        std::optional<std::int64_t> on_behalf_of;
    };

    struct HistoryQuery
    {
        int limit{20};
        std::optional<std::string> cursor;
        std::optional<std::string> market;
        std::optional<std::int64_t> on_behalf_of;
    };

    const char *to_string(Side side);
    const char *to_string(OrderType type);
    const char *to_string(AllowanceType type);
    Side side_from_int(int side);
    OrderType order_type_from_string(const std::string &type);
}
