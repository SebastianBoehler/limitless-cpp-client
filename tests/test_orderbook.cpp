#include "check.hpp"
#include "limitless/orderbook.hpp"

#include <nlohmann/json.hpp>

int main()
{
    const auto body = nlohmann::json::parse(R"({
        "bids": [{"price": 0.53, "size": 100000000, "side": "BUY"}],
        "asks": [{"price": 0.55, "size": 80000000, "side": "SELL"}],
        "tokenId": "yes-token",
        "adjustedMidpoint": 0.54,
        "lastTradePrice": 0.54,
        "maxSpread": "0.035",
        "midpoint": 0.54,
        "minSize": "100000000"
    })");
    auto book = limitless::OrderBook::parse_rest(body);
    CHECK(book.best_bid() != nullptr);
    CHECK(book.best_ask() != nullptr);
    CHECK(book.best_bid()->price == 0.53);
    CHECK(book.best_ask()->size_raw == "80000000");
    CHECK(book.min_size == "100000000");
    CHECK(limitless::raw_to_shares("150000000") == "150");
    CHECK(limitless::raw_to_shares("1000000") == "1");
    CHECK(limitless::raw_to_shares("1") == "0.000001");

    const auto no_book = limitless::derive_no_book(book);
    CHECK(no_book.best_bid() != nullptr);
    CHECK(no_book.best_bid()->price > 0.44 && no_book.best_bid()->price < 0.46);
    CHECK(no_book.best_bid()->side == "BUY");
    CHECK(no_book.best_ask()->side == "SELL");
    CHECK(no_book.best_bid()->size_raw == "80000000");
    CHECK(no_book.best_ask()->size_raw == "100000000");
    CHECK(no_book.token_id.empty());

    const auto live = nlohmann::json::parse(R"({
        "marketSlug": "btc",
        "version": 2,
        "orderbook": {
            "bids": [{"price": 0.4, "size": 1000000}],
            "asks": [{"price": 0.6, "size": 1000000}],
            "midpoint": 0.5,
            "adjustedMidpoint": 0.5,
            "tokenId": "yes-token",
            "minSize": 100000000,
            "maxSpread": 0.035
        }
    })");
    CHECK(book.apply_update(live));
    CHECK(book.seen_live_frame);
    CHECK(book.market_slug == "btc");
    CHECK(book.best_bid()->price == 0.4);

    auto stale = live;
    stale["version"] = 1;
    CHECK(!book.apply_update(stale));
    CHECK(book.best_bid()->price == 0.4);

    auto fallback = live;
    fallback["version"] = 0;
    CHECK(!book.apply_update(fallback));
    RETURN_TEST();
}
