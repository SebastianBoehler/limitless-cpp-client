#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace limitless::detail
{
    inline std::string url_encode(std::string_view value)
    {
        static constexpr char kHex[] = "0123456789ABCDEF";
        std::string out;
        out.reserve(value.size());
        for (unsigned char character : value)
        {
            const bool unreserved = (character >= 'a' && character <= 'z') ||
                                    (character >= 'A' && character <= 'Z') ||
                                    (character >= '0' && character <= '9') ||
                                    character == '-' || character == '_' || character == '.' || character == '~';
            if (unreserved)
            {
                out.push_back(static_cast<char>(character));
            }
            else
            {
                out.push_back('%');
                out.push_back(kHex[character >> 4]);
                out.push_back(kHex[character & 0x0f]);
            }
        }
        return out;
    }

    class Query
    {
    public:
        void add(std::string key, std::string value)
        {
            items_.emplace_back(std::move(key), std::move(value));
        }

        void add_opt(const char *key, const std::optional<int> &value)
        {
            if (value)
            {
                add(key, std::to_string(*value));
            }
        }

        void add_opt(const char *key, const std::optional<std::int64_t> &value)
        {
            if (value)
            {
                add(key, std::to_string(*value));
            }
        }

        void add_opt(const char *key, const std::optional<std::string> &value)
        {
            if (value && !value->empty())
            {
                add(key, *value);
            }
        }

        void add_bool(const char *key, const std::optional<bool> &value)
        {
            if (value)
            {
                add(key, *value ? "true" : "false");
            }
        }

        std::string str() const
        {
            if (items_.empty())
            {
                return {};
            }
            std::string out = "?";
            for (std::size_t index = 0; index < items_.size(); ++index)
            {
                if (index != 0)
                {
                    out.push_back('&');
                }
                out += url_encode(items_[index].first);
                out.push_back('=');
                out += url_encode(items_[index].second);
            }
            return out;
        }

    private:
        std::vector<std::pair<std::string, std::string>> items_;
    };
}
