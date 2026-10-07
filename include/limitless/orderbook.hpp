#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace limitless
{
    struct BookLevel
    {
        double price{0};
        std::string size_raw;
        std::string side;
    };

    // YES-side book from GET /markets/{slug}/orderbook or an orderbookUpdate frame.
    // Sizes stay in raw 6-decimal units. Derive the NO book with derive_no_book().
    class OrderBook
    {
    public:
        std::vector<BookLevel> bids;
        std::vector<BookLevel> asks;
        std::string token_id;
        double midpoint{0};
        double adjusted_midpoint{0};
        std::string min_size;
        std::string max_spread;
        std::optional<double> last_trade_price;
        std::string market_slug;
        std::optional<std::int64_t> version;
        bool seen_live_frame{false};

        static OrderBook parse_rest(const nlohmann::json &body);
        static OrderBook parse_update(const nlohmann::json &frame);

        // Drops a lower version after a live frame. Version 0 is a database fallback
        // and is kept only before any live frame for this slug.
        bool apply_update(const nlohmann::json &frame);

        const BookLevel *best_bid() const;
        const BookLevel *best_ask() const;
        std::optional<double> spread() const;
    };

    OrderBook derive_no_book(const OrderBook &yes_book);
    std::string raw_to_shares(const std::string &size_raw);
}
