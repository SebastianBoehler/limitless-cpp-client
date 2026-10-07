#pragma once

#include "limitless/types.hpp"

#include <string>
#include <string_view>

namespace limitless
{
    struct ScaledAmounts
    {
        std::string maker_amount;
        std::string taker_amount;
        int price_millis{0};
        std::string price_text;
    };

    // Human decimal ("10", "10.5", "0.000001") to a raw 6-decimal integer string.
    // Rejects more than 6 fractional digits.
    std::string scale_to_raw_1e6(std::string_view human);

    // "0.5" / "0.50" / "0.500" -> 500. Range is 0.010 through 0.990, at most 3 places.
    int price_to_millis(std::string_view price);
    std::string format_price_millis(int price_millis);

    // GTC and FAK share the price/size formulas. FOK uses fok_amounts().
    ScaledAmounts limit_amounts(Side side, std::string_view price, std::string_view shares);
    ScaledAmounts fok_amounts(Side side, std::string_view human_amount);

    // YES price p and NO price 1-p, in the same milli-price units (0.001).
    int complementary_price_millis(int yes_price_millis);
}
