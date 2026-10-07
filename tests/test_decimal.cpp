#include "check.hpp"
#include "limitless/decimal_math.hpp"

#include <stdexcept>

int main()
{
    CHECK(limitless::scale_to_raw_1e6("10") == "10000000");
    CHECK(limitless::scale_to_raw_1e6("10.5") == "10500000");
    CHECK(limitless::scale_to_raw_1e6("0.000001") == "1");
    CHECK(limitless::price_to_millis("0.5") == 500);
    CHECK(limitless::price_to_millis("0.50") == 500);
    CHECK(limitless::price_to_millis("0.333") == 333);
    CHECK(limitless::format_price_millis(500) == "0.5");
    CHECK(limitless::format_price_millis(10) == "0.01");
    CHECK(limitless::format_price_millis(333) == "0.333");

    const auto buy = limitless::limit_amounts(limitless::Side::Buy, "0.50", "10");
    CHECK(buy.maker_amount == "5000000");
    CHECK(buy.taker_amount == "10000000");
    CHECK(buy.price_text == "0.5");

    const auto sell = limitless::limit_amounts(limitless::Side::Sell, "0.50", "10");
    CHECK(sell.maker_amount == "10000000");
    CHECK(sell.taker_amount == "5000000");

    const auto odd = limitless::limit_amounts(limitless::Side::Buy, "0.333", "2");
    CHECK(odd.maker_amount == "666000");
    CHECK(odd.taker_amount == "2000000");

    const auto fok = limitless::fok_amounts(limitless::Side::Buy, "1.25");
    CHECK(fok.maker_amount == "1250000");
    CHECK(fok.taker_amount == "1");
    CHECK(limitless::complementary_price_millis(500) == 500);
    CHECK(limitless::complementary_price_millis(330) == 670);

    bool rejected = false;
    try
    {
        limitless::price_to_millis("0.3333");
    }
    catch (const std::invalid_argument &)
    {
        rejected = true;
    }
    CHECK(rejected);

    rejected = false;
    try
    {
        limitless::limit_amounts(limitless::Side::Buy, "0.001", "10");
    }
    catch (const std::invalid_argument &)
    {
        rejected = true;
    }
    CHECK(rejected);
    RETURN_TEST();
}
