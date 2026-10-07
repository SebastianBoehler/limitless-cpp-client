#include "limitless/orderbook.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace limitless
{
    namespace
    {
        std::string raw_from_json(const nlohmann::json &value)
        {
            if (value.is_string())
            {
                return value.get<std::string>();
            }
            if (value.is_number_unsigned())
            {
                return std::to_string(value.get<std::uint64_t>());
            }
            if (value.is_number_integer())
            {
                return std::to_string(value.get<std::int64_t>());
            }
            if (value.is_number_float())
            {
                const double number = value.get<double>();
                if (!std::isfinite(number))
                {
                    throw std::invalid_argument("orderbook size is not finite");
                }
                return std::to_string(static_cast<long long>(std::llround(number)));
            }
            throw std::invalid_argument("orderbook size has an unexpected type");
        }

        BookLevel parse_level(const nlohmann::json &value, const char *fallback_side)
        {
            if (!value.is_object() || !value.contains("price") || !value.contains("size"))
            {
                throw std::invalid_argument("orderbook level is missing price or size");
            }
            BookLevel level;
            level.price = value["price"].get<double>();
            level.size_raw = raw_from_json(value["size"]);
            if (value.contains("side") && value["side"].is_string())
            {
                level.side = value["side"].get<std::string>();
            }
            else
            {
                level.side = fallback_side;
            }
            return level;
        }

        std::vector<BookLevel> parse_levels(const nlohmann::json &value, const char *fallback_side)
        {
            std::vector<BookLevel> levels;
            if (!value.is_array())
            {
                throw std::invalid_argument("orderbook side is not an array");
            }
            levels.reserve(value.size());
            for (const auto &level : value)
            {
                levels.push_back(parse_level(level, fallback_side));
            }
            return levels;
        }

        void sort_book(OrderBook &book)
        {
            std::sort(book.bids.begin(), book.bids.end(), [](const BookLevel &left, const BookLevel &right) {
                return left.price > right.price;
            });
            std::sort(book.asks.begin(), book.asks.end(), [](const BookLevel &left, const BookLevel &right) {
                return left.price < right.price;
            });
        }

        void read_common(OrderBook &book, const nlohmann::json &body)
        {
            book.bids = parse_levels(body.at("bids"), "BUY");
            book.asks = parse_levels(body.at("asks"), "SELL");
            if (body.contains("tokenId") && body["tokenId"].is_string())
            {
                book.token_id = body["tokenId"].get<std::string>();
            }
            if (body.contains("midpoint") && body["midpoint"].is_number())
            {
                book.midpoint = body["midpoint"].get<double>();
            }
            if (body.contains("adjustedMidpoint") && body["adjustedMidpoint"].is_number())
            {
                book.adjusted_midpoint = body["adjustedMidpoint"].get<double>();
            }
            if (body.contains("minSize"))
            {
                book.min_size = raw_from_json(body["minSize"]);
            }
            if (body.contains("maxSpread"))
            {
                if (body["maxSpread"].is_string())
                {
                    book.max_spread = body["maxSpread"].get<std::string>();
                }
                else if (body["maxSpread"].is_number())
                {
                    book.max_spread = body["maxSpread"].dump();
                }
            }
            if (body.contains("lastTradePrice") && !body["lastTradePrice"].is_null() &&
                body["lastTradePrice"].is_number())
            {
                book.last_trade_price = body["lastTradePrice"].get<double>();
            }
            sort_book(book);
        }
    }

    OrderBook OrderBook::parse_rest(const nlohmann::json &body)
    {
        OrderBook book;
        read_common(book, body);
        return book;
    }

    OrderBook OrderBook::parse_update(const nlohmann::json &frame)
    {
        if (!frame.is_object() || !frame.contains("orderbook"))
        {
            throw std::invalid_argument("orderbookUpdate is missing orderbook");
        }
        OrderBook book;
        read_common(book, frame["orderbook"]);
        if (frame.contains("marketSlug") && frame["marketSlug"].is_string())
        {
            book.market_slug = frame["marketSlug"].get<std::string>();
        }
        if (frame.contains("version") && frame["version"].is_number())
        {
            book.version = frame["version"].get<std::int64_t>();
        }
        return book;
    }

    bool OrderBook::apply_update(const nlohmann::json &frame)
    {
        if (!frame.contains("version") || !frame["version"].is_number())
        {
            throw std::invalid_argument("orderbookUpdate is missing version");
        }
        const auto incoming = frame["version"].get<std::int64_t>();
        if (incoming == 0 && seen_live_frame)
        {
            return false;
        }
        if (incoming > 0 && seen_live_frame && version && incoming < *version)
        {
            return false;
        }
        const bool live = seen_live_frame || incoming > 0;
        OrderBook next = parse_update(frame);
        next.seen_live_frame = live;
        if (incoming == 0)
        {
            next.version = version;
        }
        *this = std::move(next);
        return true;
    }

    const BookLevel *OrderBook::best_bid() const
    {
        return bids.empty() ? nullptr : &bids.front();
    }

    const BookLevel *OrderBook::best_ask() const
    {
        return asks.empty() ? nullptr : &asks.front();
    }

    std::optional<double> OrderBook::spread() const
    {
        if (!best_bid() || !best_ask())
        {
            return std::nullopt;
        }
        return best_ask()->price - best_bid()->price;
    }

    OrderBook derive_no_book(const OrderBook &yes_book)
    {
        OrderBook no_book;
        no_book.market_slug = yes_book.market_slug;
        no_book.min_size = yes_book.min_size;
        no_book.max_spread = yes_book.max_spread;
        no_book.midpoint = 1.0 - yes_book.midpoint;
        no_book.adjusted_midpoint = 1.0 - yes_book.adjusted_midpoint;
        no_book.bids.reserve(yes_book.asks.size());
        for (const auto &level : yes_book.asks)
        {
            BookLevel inverted = level;
            inverted.price = 1.0 - level.price;
            inverted.side = "BUY";
            no_book.bids.push_back(std::move(inverted));
        }
        no_book.asks.reserve(yes_book.bids.size());
        for (const auto &level : yes_book.bids)
        {
            BookLevel inverted = level;
            inverted.price = 1.0 - level.price;
            inverted.side = "SELL";
            no_book.asks.push_back(std::move(inverted));
        }
        std::sort(no_book.bids.begin(), no_book.bids.end(), [](const BookLevel &left, const BookLevel &right) {
            return left.price > right.price;
        });
        std::sort(no_book.asks.begin(), no_book.asks.end(), [](const BookLevel &left, const BookLevel &right) {
            return left.price < right.price;
        });
        return no_book;
    }

    std::string raw_to_shares(const std::string &size_raw)
    {
        std::string digits = size_raw.empty() ? "0" : size_raw;
        for (char character : digits)
        {
            if (character < '0' || character > '9')
            {
                throw std::invalid_argument("raw size must be an integer string");
            }
        }
        if (digits.size() <= 6)
        {
            digits.insert(0, 7 - digits.size(), '0');
        }
        digits.insert(digits.size() - 6, 1, '.');
        while (!digits.empty() && digits.back() == '0')
        {
            digits.pop_back();
        }
        if (!digits.empty() && digits.back() == '.')
        {
            digits.pop_back();
        }
        return digits;
    }
}
