#include "limitless/clob_client.hpp"

#include <algorithm>
#include <iostream>
#include <string>

namespace
{
    void print_error(const limitless::SdkError &error)
    {
        std::cerr << limitless::sdk_error_code_to_string(error.code);
        if (error.http_status != 0)
        {
            std::cerr << " HTTP " << error.http_status;
        }
        if (!error.endpoint.empty())
        {
            std::cerr << " " << error.endpoint;
        }
        std::cerr << ": " << error.message << "\n";
    }

    void print_level(const char *label, const limitless::BookLevel *level)
    {
        if (!level)
        {
            std::cout << label << ": none\n";
            return;
        }
        std::cout << label << ": price " << level->price << " size " << limitless::raw_to_shares(level->size_raw)
                  << " shares (" << level->size_raw << " raw)\n";
    }

    int print_book(const limitless::MarketSummary *market, const limitless::OrderBook &book)
    {
        if (market)
        {
            std::cout << market->title << "\n";
            std::cout << "slug " << market->slug << " tradeType " << market->trade_type << " status " << market->status
                      << "\n";
            if (market->venue)
            {
                std::cout << "venue.exchange " << market->venue->exchange << "\n";
            }
            if (market->tokens)
            {
                std::cout << "YES " << market->tokens->yes << "\n";
                std::cout << "NO  " << market->tokens->no << "\n";
            }
        }
        else
        {
            std::cout << "slug " << book.market_slug << "\n";
        }
        std::cout << "YES token " << book.token_id << "\n";
        std::cout << "midpoint " << book.midpoint << " adjustedMidpoint " << book.adjusted_midpoint << "\n";
        if (book.last_trade_price)
        {
            std::cout << "lastTradePrice " << *book.last_trade_price << "\n";
        }
        print_level("best YES bid", book.best_bid());
        print_level("best YES ask", book.best_ask());
        const auto no_book = limitless::derive_no_book(book);
        print_level("best NO bid", no_book.best_bid());
        print_level("best NO ask", no_book.best_ask());
        std::cout << "bids " << book.bids.size() << " asks " << book.asks.size() << "\n";
        const auto levels = std::min<std::size_t>(book.asks.size(), 5);
        for (std::size_t index = 0; index < levels; ++index)
        {
            const auto &level = book.asks[index];
            std::cout << "  ask " << level.price << " x " << limitless::raw_to_shares(level.size_raw) << "\n";
        }
        return 0;
    }
}

int main(int argc, char **argv)
{
    std::string slug;
    int limit = 5;
    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index];
        if (argument == "--limit" && index + 1 < argc)
        {
            limit = std::stoi(argv[++index]);
        }
        else if (!argument.empty() && argument[0] != '-')
        {
            slug = argument;
        }
        else
        {
            std::cerr << "usage: limitless_markets_orderbook [slug] [--limit N]\n";
            return 2;
        }
    }

    limitless::ClobClient client(limitless::Environment::base());
    if (!slug.empty())
    {
        const auto market = client.get_market(slug);
        const auto book = client.get_orderbook(slug);
        if (!book)
        {
            print_error(book.error());
            return 1;
        }
        if (!market)
        {
            print_error(market.error());
            return print_book(nullptr, book.value());
        }
        return print_book(&market.value(), book.value());
    }

    limitless::ActiveMarketsQuery query;
    query.limit = limit;
    query.trade_type = "clob";
    const auto page = client.get_active_markets(query);
    if (!page)
    {
        print_error(page.error());
        return 1;
    }
    std::cout << "active CLOB markets returned: " << page.value().markets.size() << "\n";
    for (const auto &market : page.value().markets)
    {
        std::cout << "- " << market.slug << " [" << market.trade_type << "] " << market.title << "\n";
        const auto book = client.get_orderbook(market.slug);
        if (!book)
        {
            std::cout << "  orderbook skipped: " << book.error().message << "\n";
            continue;
        }
        return print_book(&market, book.value());
    }
    std::cerr << "no CLOB orderbook in the first page\n";
    return 1;
}
