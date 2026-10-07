#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace limitless {

struct BookLevel {
    double price = 0;
    double size = 0;
    std::string side;
};

struct Orderbook {
    std::vector<BookLevel> bids;
    std::vector<BookLevel> asks;
    std::string token_id;
    double midpoint = 0;
    double adjusted_midpoint = 0;
    std::string min_size;
    std::string max_spread;
    std::optional<double> last_trade_price;
};

Orderbook parse_orderbook(const nlohmann::json &json);
Orderbook derive_no_book(const Orderbook &yes_book);

// Replaces the local book with each orderbookUpdate snapshot.
// Drops a frame whose version is lower than the last live version for that slug.
// A database fallback snapshot (version 0) is kept only before any live frame.
class OrderbookManager {
public:
    bool apply_update(const nlohmann::json &frame);
    const Orderbook *book(const std::string &slug) const;
    std::optional<std::int64_t> version(const std::string &slug) const;

private:
    struct Entry {
        Orderbook book;
        std::int64_t version = 0;
        bool saw_live = false;
    };
    std::unordered_map<std::string, Entry> books_;
};

}  // namespace limitless
