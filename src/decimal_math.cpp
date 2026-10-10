#include "limitless/decimal_math.hpp"

#include <limits>
#include <stdexcept>

namespace limitless
{
    namespace
    {
        std::string strip_leading_zeros(std::string value)
        {
            std::size_t index = 0;
            while (index + 1 < value.size() && value[index] == '0')
            {
                ++index;
            }
            return value.substr(index);
        }

        void require_digits(std::string_view text, const char *what)
        {
            if (text.empty())
            {
                throw std::invalid_argument(std::string(what) + " is empty");
            }
            for (char character : text)
            {
                if (character < '0' || character > '9')
                {
                    throw std::invalid_argument(std::string(what) + " must be a decimal integer");
                }
            }
        }
    }

    std::string scale_to_raw_1e6(std::string_view human)
    {
        if (human.empty() || human.front() == '+' || human.front() == '-')
        {
            throw std::invalid_argument("amount must be a non-negative decimal");
        }
        const auto dot = human.find('.');
        std::string_view whole = dot == std::string_view::npos ? human : human.substr(0, dot);
        std::string_view frac = dot == std::string_view::npos ? std::string_view{} : human.substr(dot + 1);
        if (whole.empty() || frac.size() > static_cast<std::size_t>(kCollateralDecimals))
        {
            throw std::invalid_argument("amount must have at most 6 decimal places");
        }
        require_digits(whole, "amount");
        if (!frac.empty())
        {
            require_digits(frac, "amount");
        }
        std::string raw(whole);
        raw.append(frac);
        raw.append(static_cast<std::size_t>(kCollateralDecimals) - frac.size(), '0');
        return strip_leading_zeros(raw);
    }

    int price_to_millis(std::string_view price)
    {
        if (price.size() < 3 || price[0] != '0' || price[1] != '.')
        {
            throw std::invalid_argument("price must look like 0.01 through 0.99");
        }
        const auto frac = price.substr(2);
        if (frac.empty() || frac.size() > static_cast<std::size_t>(kPriceDecimals))
        {
            throw std::invalid_argument("price must have 1 to 3 decimal places");
        }
        require_digits(frac, "price");
        int millis = 0;
        for (char character : frac)
        {
            millis = millis * 10 + (character - '0');
        }
        for (std::size_t pad = frac.size(); pad < static_cast<std::size_t>(kPriceDecimals); ++pad)
        {
            millis *= 10;
        }
        if (millis < 10 || millis > 990)
        {
            throw std::invalid_argument("price must be from 0.01 through 0.99");
        }
        return millis;
    }

    std::string format_price_millis(int price_millis)
    {
        if (price_millis < 10 || price_millis > 990)
        {
            throw std::invalid_argument("price millis out of range");
        }
        std::string text = "0.";
        const int hundreds = price_millis / 100;
        const int tens = (price_millis / 10) % 10;
        const int ones = price_millis % 10;
        text.push_back(static_cast<char>('0' + hundreds));
        text.push_back(static_cast<char>('0' + tens));
        text.push_back(static_cast<char>('0' + ones));
        while (text.back() == '0')
        {
            text.pop_back();
        }
        return text;
    }

    namespace
    {
        std::string multiply_raw_by_millis(const std::string &raw, int millis)
        {
            unsigned long long value = 0;
            for (char character : raw)
            {
                const auto digit = static_cast<unsigned>(character - '0');
                if (value > (std::numeric_limits<unsigned long long>::max() - digit) / 10u)
                {
                    throw std::invalid_argument("amount overflows");
                }
                value = value * 10u + digit;
            }
            if (millis < 0)
            {
                throw std::invalid_argument("price millis out of range");
            }
            const unsigned long long product = value * static_cast<unsigned>(millis);
            if (millis != 0 && product / static_cast<unsigned>(millis) != value)
            {
                throw std::invalid_argument("amount overflows");
            }
            if (product % 1000u != 0u)
            {
                throw std::invalid_argument("price × size is not an integer raw amount");
            }
            return std::to_string(product / 1000u);
        }
    }

    ScaledAmounts limit_amounts(Side side, std::string_view price, std::string_view shares)
    {
        ScaledAmounts amounts;
        amounts.price_millis = price_to_millis(price);
        amounts.price_text = format_price_millis(amounts.price_millis);
        const std::string contracts = scale_to_raw_1e6(shares);
        if (contracts == "0")
        {
            throw std::invalid_argument("size must be greater than 0");
        }
        const std::string collateral = multiply_raw_by_millis(contracts, amounts.price_millis);
        if (side == Side::Buy)
        {
            amounts.maker_amount = collateral;
            amounts.taker_amount = contracts;
        }
        else
        {
            amounts.maker_amount = contracts;
            amounts.taker_amount = collateral;
        }
        if (amounts.maker_amount.size() < 3 || (amounts.maker_amount.size() == 3 && amounts.maker_amount < "100"))
        {
            throw std::invalid_argument("makerAmount must be at least 100 raw units");
        }
        return amounts;
    }

    ScaledAmounts fok_amounts(Side side, std::string_view human_amount)
    {
        (void)side;
        ScaledAmounts amounts;
        amounts.maker_amount = scale_to_raw_1e6(human_amount);
        amounts.taker_amount = "1";
        if (amounts.maker_amount.size() < 3 || (amounts.maker_amount.size() == 3 && amounts.maker_amount < "100"))
        {
            throw std::invalid_argument("makerAmount must be at least 100 raw units");
        }
        return amounts;
    }

    int complementary_price_millis(int yes_price_millis)
    {
        if (yes_price_millis < 0 || yes_price_millis > 1000)
        {
            throw std::invalid_argument("price millis out of range");
        }
        return 1000 - yes_price_millis;
    }
}
