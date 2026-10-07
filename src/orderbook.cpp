#include "limitless/orderbook.hpp"

#include <algorithm>

namespace limitless {
namespace {

std::string numberish(const nlohmann::json &value) {
    if (value.is_string()) return value.get<std::string>();
    if (value.is_number()) return value.dump();
    return {};
}

BookLevel parse_level(const nlohmann::json &value) {
    BookLevel level;
    level.price = value.value("price", 0.0);
    level.size = value.value("size", 0.0);
    level.side = value.value("side", "");
    return level;
}

}  // namespace

Orderbook parse_orderbook(const nlohmann::json &json) {
    Orderbook book;
    const nlohmann::json &src = json.contains("orderbook") && json["orderbook"].is_object()
                                    ? json["orderbook"]
                                    : json;
    if (src.contains("bids") && src["bids"].is_array()) {
        for (const auto &level : src["bids"]) book.bids.push_back(parse_level(level));
    }
    if (src.contains("asks") && src["asks"].is_array()) {
        for (const auto &level : src["asks"]) book.asks.push_back(parse_level(level));
    }
    if (src.contains("tokenId") && src["tokenId"].is_string()) book.token_id = src["tokenId"];
    book.midpoint = src.value("midpoint", 0.0);
    book.adjusted_midpoint = src.value("adjustedMidpoint", 0.0);
    if (src.contains("minSize")) book.min_size = numberish(src["minSize"]);
    if (src.contains("maxSpread")) book.max_spread = numberish(src["maxSpread"]);
    if (src.contains("lastTradePrice") && src["lastTradePrice"].is_number()) {
        book.last_trade_price = src["lastTradePrice"].get<double>();
    }
    return book;
}

Orderbook derive_no_book(const Orderbook &yes_book) {
    Orderbook no;
    no.token_id = yes_book.token_id;
    no.min_size = yes_book.min_size;
    no.max_spread = yes_book.max_spread;
    no.midpoint = 1.0 - yes_book.midpoint;
    no.adjusted_midpoint = 1.0 - yes_book.adjusted_midpoint;
    if (yes_book.last_trade_price) no.last_trade_price = 1.0 - *yes_book.last_trade_price;

    auto invert = [](BookLevel level, const char *side) {
        level.price = 1.0 - level.price;
        level.side = side;
        return level;
    };
    for (const auto &ask : yes_book.asks) no.bids.push_back(invert(ask, "BUY"));
    for (const auto &bid : yes_book.bids) no.asks.push_back(invert(bid, "SELL"));
    std::sort(no.bids.begin(), no.bids.end(),
              [](const BookLevel &a, const BookLevel &b) { return a.price > b.price; });
    std::sort(no.asks.begin(), no.asks.end(),
              [](const BookLevel &a, const BookLevel &b) { return a.price < b.price; });
    return no;
}

bool OrderbookManager::apply_update(const nlohmann::json &frame) {
    if (!frame.contains("marketSlug") || !frame["marketSlug"].is_string()) return false;
    const std::string slug = frame["marketSlug"];
    const std::int64_t version = frame.value("version", static_cast<std::int64_t>(0));
    Entry &entry = books_[slug];
    if (version == 0) {
        if (entry.saw_live) return false;
    } else if (entry.saw_live && version < entry.version) {
        return false;
    }
    entry.book = parse_orderbook(frame);
    entry.version = version;
    if (version != 0) entry.saw_live = true;
    return true;
}

const Orderbook *OrderbookManager::book(const std::string &slug) const {
    const auto it = books_.find(slug);
    if (it == books_.end()) return nullptr;
    return &it->second.book;
}

std::optional<std::int64_t> OrderbookManager::version(const std::string &slug) const {
    const auto it = books_.find(slug);
    if (it == books_.end()) return std::nullopt;
    return it->second.version;
}

}  // namespace limitless
