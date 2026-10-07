#include "limitless/clob_client.hpp"
#include "limitless/orderbook.hpp"

#include <iostream>

int main() {
    limitless::ClobClient client(limitless::Environment::production());
    auto warmed = client.warm_connection();
    if (!warmed) {
        std::cerr << warmed.error().message << "\n";
        return 1;
    }

    limitless::ActiveMarketsQuery query;
    query.limit = 8;
    query.trade_type = "clob";
    auto markets = client.get_active_markets(query);
    if (!markets) {
        std::cerr << "GET /markets/active failed: " << markets.error().message << "\n";
        return 1;
    }

    std::cout << "active markets (filtered page): " << markets.value().data.size()
              << " total=" << markets.value().total_markets_count << "\n";
    for (const auto &market : markets.value().data) {
        std::cout << market.slug << "  " << market.trade_type << "  " << market.title << "\n";
        std::cout << "  venue.exchange=" << market.venue.exchange << "\n";
    }

    for (const auto &market : markets.value().data) {
        if (market.trade_type != "clob" || market.slug.empty()) continue;
        auto book = client.get_orderbook(market.slug);
        if (!book) {
            std::cerr << "skip " << market.slug << ": " << book.error().message << "\n";
            continue;
        }
        const auto &yes = book.value();
        std::cout << "orderbook " << market.slug << " token=" << yes.token_id
                  << " midpoint=" << yes.midpoint << " bids=" << yes.bids.size()
                  << " asks=" << yes.asks.size() << "\n";
        if (!yes.bids.empty()) {
            std::cout << "  best YES bid " << yes.bids.front().price << " size_raw " << yes.bids.front().size
                      << "\n";
        }
        if (!yes.asks.empty()) {
            std::cout << "  best YES ask " << yes.asks.front().price << " size_raw " << yes.asks.front().size
                      << "\n";
        }
        const auto no = limitless::derive_no_book(yes);
        if (!no.bids.empty()) std::cout << "  best NO bid " << no.bids.front().price << "\n";
        if (!no.asks.empty()) std::cout << "  best NO ask " << no.asks.front().price << "\n";
        return 0;
    }
    std::cerr << "no live CLOB orderbook in the first page\n";
    return 1;
}
