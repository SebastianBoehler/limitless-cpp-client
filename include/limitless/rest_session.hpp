#pragma once

#include "limitless/auth.hpp"
#include "limitless/environment.hpp"
#include "limitless/http_client.hpp"
#include "limitless/network.hpp"
#include "limitless/sdk_error.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace limitless
{
    class RestSession
    {
    public:
        explicit RestSession(Environment environment, NetworkOptions network = {});

        void set_credentials(HmacCredentials credentials);
        void clear_credentials();
        bool has_credentials() const;

        const Environment &environment() const;
        const HmacCredentials *credentials() const;

        Result<nlohmann::json> get(const std::string &path_and_query);
        Result<nlohmann::json> request(std::string_view method,
                                       const std::string &path_and_query,
                                       const std::string &body = {},
                                       std::vector<std::pair<std::string, std::string>> headers = {});

    private:
        Environment environment_;
        HttpClient http_;
        std::optional<HmacCredentials> credentials_;
    };
}
