#include "limitless/types.hpp"

#include <stdexcept>

namespace limitless
{
    const char *to_string(Side side)
    {
        return side == Side::Buy ? "BUY" : "SELL";
    }

    const char *to_string(OrderType type)
    {
        switch (type)
        {
        case OrderType::Gtc:
            return "GTC";
        case OrderType::Fak:
            return "FAK";
        case OrderType::Fok:
            return "FOK";
        }
        return "GTC";
    }

    const char *to_string(AllowanceType type)
    {
        return type == AllowanceType::Clob ? "clob" : "negrisk";
    }

    Side side_from_int(int side)
    {
        if (side == 0)
        {
            return Side::Buy;
        }
        if (side == 1)
        {
            return Side::Sell;
        }
        throw std::invalid_argument("side must be 0 (BUY) or 1 (SELL)");
    }

    OrderType order_type_from_string(const std::string &type)
    {
        if (type == "GTC")
        {
            return OrderType::Gtc;
        }
        if (type == "FAK")
        {
            return OrderType::Fak;
        }
        if (type == "FOK")
        {
            return OrderType::Fok;
        }
        throw std::invalid_argument("orderType must be GTC, FAK, or FOK");
    }
}
