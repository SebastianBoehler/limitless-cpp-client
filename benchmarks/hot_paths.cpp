#include "limitless/limitless.hpp"
#include <chrono>
#include <cstdint>
#include <iostream>

namespace
{
    volatile std::size_t sink = 0;
    template <typename F> void measure(const char *name, int iterations, F operation)
    {
        for (int i = 0; i < 100; ++i)
            operation();
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i)
            operation();
        const auto end = std::chrono::steady_clock::now();
        std::cout << name << ": " << std::chrono::duration<double, std::micro>(end - start).count() / iterations
                  << " us/op\n";
    }
} // namespace

int main()
{
    nlohmann::json payload = {{"bids", nlohmann::json::array()}, {"asks", nlohmann::json::array()}};
    for (int i = 0; i < 100; ++i)
    {
        payload["bids"].push_back({{"price", 0.499 - i * 0.001}, {"size", "100000000"}});
        payload["asks"].push_back({{"price", 0.501 + i * 0.001}, {"size", "100000000"}});
    }
    const auto book = limitless::OrderBook::parse_rest(payload);
    measure("derive_no_book_100_levels", 20000, [&] { sink = limitless::derive_no_book(book).bids.size(); });
    measure("parse_book_100_levels", 5000, [&] { sink = limitless::OrderBook::parse_rest(payload).bids.size(); });
}
