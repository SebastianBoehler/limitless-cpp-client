#pragma once

#include "limitless/types.hpp"

#include <string>

namespace limitless {

bool scale_decimal(std::string_view human, int scale, std::string &out, std::string &error,
                   bool allow_zero = false);

bool gtc_amounts(OrderSide side, std::string_view price, std::string_view size_shares,
                 ScaledAmounts &out, std::string &error);

bool fok_amounts(OrderSide side, std::string_view human_amount, ScaledAmounts &out,
                 std::string &error);

}  // namespace limitless
