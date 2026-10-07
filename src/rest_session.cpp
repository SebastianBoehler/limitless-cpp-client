#include "limitless/rest_session.hpp"

namespace limitless
{
    RestSession::RestSession(Environment environment, NetworkOptions network)
        : environment_(std::move(environment)),
          http_(std::move(network))
    {
        environment_.validate();
    }

    void RestSession::set_credentials(HmacCredentials credentials)
    {
        credentials_ = std::move(credentials);
    }

    void RestSession::clear_credentials()
    {
        credentials_.reset();
    }

    bool RestSession::has_credentials() const
    {
        return credentials_.has_value();
    }

    const Environment &RestSession::environment() const
    {
        return environment_;
    }

    const HmacCredentials *RestSession::credentials() const
    {
        return credentials_ ? &*credentials_ : nullptr;
    }

    Result<nlohmann::json> RestSession::get(const std::string &path_and_query)
    {
        return request("GET", path_and_query);
    }

    Result<nlohmann::json> RestSession::request(std::string_view method,
                                                const std::string &path_and_query,
                                                const std::string &body,
                                                std::vector<std::pair<std::string, std::string>> headers)
    {
        if (path_and_query.empty() || path_and_query.front() != '/')
        {
            return Result<nlohmann::json>::failure(make_invalid_argument("path must start with /"));
        }
        bool has_content_type = false;
        bool sign = true;
        std::vector<std::pair<std::string, std::string>> outbound;
        outbound.reserve(headers.size() + 6);
        for (auto &header : headers)
        {
            if (header.first == "X-Limitless-Skip-Hmac")
            {
                sign = false;
                continue;
            }
            if (header.first == "Content-Type" || header.first == "content-type")
            {
                has_content_type = true;
            }
            outbound.push_back(std::move(header));
        }
        if (!body.empty() && !has_content_type)
        {
            outbound.emplace_back("Content-Type", "application/json");
        }
        outbound.emplace_back("Accept", "application/json");
        if (sign && credentials_)
        {
            try
            {
                const auto signed_headers = sign_request_now(*credentials_, method, path_and_query, body);
                for (auto &field : signed_headers.fields())
                {
                    outbound.push_back(std::move(field));
                }
            }
            catch (const std::exception &error)
            {
                return Result<nlohmann::json>::failure(make_signing_error(error.what()));
            }
        }
        const std::string url = environment_.rest_url + path_and_query;
        const auto response = http_.request(std::string(method), url, body, outbound);
        if (!response.error.empty() || response.status == 0)
        {
            return Result<nlohmann::json>::failure(make_transport_error(response.error, path_and_query));
        }
        if (response.status < 200 || response.status >= 300)
        {
            return Result<nlohmann::json>::failure(
                make_http_error(response.status, response.body, path_and_query, response.retry_after));
        }
        if (response.body.empty())
        {
            return Result<nlohmann::json>::success(nlohmann::json(nullptr));
        }
        auto parsed = nlohmann::json::parse(response.body, nullptr, false);
        if (parsed.is_discarded())
        {
            return Result<nlohmann::json>::failure(
                make_parse_error("response was not JSON", path_and_query, response.body));
        }
        return Result<nlohmann::json>::success(std::move(parsed));
    }
}
