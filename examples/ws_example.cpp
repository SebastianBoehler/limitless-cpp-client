#include "limitless/clob_client.hpp"
#include "limitless/orderbook.hpp"
#include "limitless/websocket_client.hpp"

#include <iostream>

int main(int argc, char **argv) {
    std::string slug;
    if (argc > 1) {
        slug = argv[1];
    } else {
        limitless::ClobClient client;
        limitless::ActiveMarketsQuery query;
        query.limit = 1;
        query.trade_type = "clob";
        auto markets = client.get_active_markets(query);
        if (!markets || markets.value().data.empty()) {
            std::cerr << "could not resolve a CLOB slug\n";
            return 1;
        }
        slug = markets.value().data.front().slug;
    }

    limitless::WebSocketClient ws;
    limitless::OrderbookManager books;
    ws.on_event([&](const limitless::SocketPacket &packet) {
        if (packet.event == "orderbookUpdate") {
            books.apply_update(packet.data);
            const auto *book = books.book(slug);
            std::cout << "orderbookUpdate " << slug;
            if (book) std::cout << " bids=" << book->bids.size() << " asks=" << book->asks.size();
            std::cout << "\n";
        } else if (packet.event == "system") {
            std::cout << "system\n";
        }
    });
    ws.subscribe_market_prices({slug});
    auto connected = ws.connect();
    if (!connected) {
        std::cerr << "Socket.IO connect failed: " << connected.error().message << "\n";
        return 1;
    }
    auto polled = ws.poll(8000);
    if (!polled) {
        std::cerr << polled.error().message << "\n";
        return 1;
    }
    std::cout << "events=" << polled.value() << "\n";
    return polled.value() > 0 ? 0 : 2;
}
