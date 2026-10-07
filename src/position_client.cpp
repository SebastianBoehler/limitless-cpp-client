#include "limitless/position_client.hpp"

#include "limitless/endpoints.hpp"
#include "query.hpp"

namespace limitless
{
    namespace
    {
        nlohmann::json operation_body(const PositionOperation &operation)
        {
            nlohmann::json venue = nlohmann::json::object();
            if (!operation.venue.exchange.empty())
            {
                venue["exchange"] = operation.venue.exchange;
            }
            if (operation.venue.adapter)
            {
                venue["adapter"] = *operation.venue.adapter;
            }
            nlohmann::json body = {{"conditionId", operation.condition_id},
                                   {"amount", operation.amount_raw},
                                   {"venue", std::move(venue)}};
            if (operation.on_behalf_of)
            {
                body["onBehalfOf"] = *operation.on_behalf_of;
            }
            return body;
        }

        std::vector<std::pair<std::string, std::string>> behalf_header(std::optional<std::int64_t> on_behalf_of)
        {
            if (!on_behalf_of)
            {
                return {};
            }
            return {{"x-on-behalf-of", std::to_string(*on_behalf_of)}};
        }
    }

    PositionClient::PositionClient(Environment environment, NetworkOptions network)
        : session_(std::move(environment), std::move(network))
    {
    }

    void PositionClient::set_credentials(HmacCredentials credentials)
    {
        session_.set_credentials(std::move(credentials));
    }

    RestSession &PositionClient::session()
    {
        return session_;
    }

    Result<nlohmann::json> PositionClient::get_positions(std::optional<std::int64_t> on_behalf_of)
    {
        return session_.request("GET", std::string(endpoints::portfolio_positions), {}, behalf_header(on_behalf_of));
    }

    Result<nlohmann::json> PositionClient::get_trades()
    {
        return session_.get(std::string(endpoints::portfolio_trades));
    }

    Result<nlohmann::json> PositionClient::get_history(const HistoryQuery &query)
    {
        detail::Query encoded;
        encoded.add("limit", std::to_string(query.limit));
        encoded.add_opt("cursor", query.cursor);
        encoded.add_opt("market", query.market);
        return session_.request("GET",
                                std::string(endpoints::portfolio_history) + encoded.str(),
                                {},
                                behalf_header(query.on_behalf_of));
    }

    Result<nlohmann::json> PositionClient::get_pnl_chart(const std::string &timeframe)
    {
        detail::Query encoded;
        if (!timeframe.empty())
        {
            encoded.add("timeframe", timeframe);
        }
        return session_.get(std::string(endpoints::portfolio_pnl_chart) + encoded.str());
    }

    Result<nlohmann::json> PositionClient::get_points()
    {
        return session_.get(std::string(endpoints::portfolio_points));
    }

    Result<nlohmann::json> PositionClient::get_allowance(AllowanceType type, const std::string &spender)
    {
        detail::Query encoded;
        encoded.add("type", to_string(type));
        if (!spender.empty())
        {
            encoded.add("spender", spender);
        }
        return session_.get(std::string(endpoints::portfolio_allowance) + encoded.str());
    }

    Result<nlohmann::json> PositionClient::redeem(const std::string &condition_id,
                                                  std::optional<std::int64_t> on_behalf_of)
    {
        nlohmann::json body = {{"conditionId", condition_id}};
        if (on_behalf_of)
        {
            body["onBehalfOf"] = *on_behalf_of;
        }
        return session_.request("POST", std::string(endpoints::portfolio_redeem), body.dump());
    }

    Result<nlohmann::json> PositionClient::split(const PositionOperation &operation)
    {
        return session_.request("POST", std::string(endpoints::portfolio_split), operation_body(operation).dump());
    }

    Result<nlohmann::json> PositionClient::merge(const PositionOperation &operation)
    {
        return session_.request("POST", std::string(endpoints::portfolio_merge), operation_body(operation).dump());
    }

    Result<nlohmann::json> PositionClient::withdraw(const std::string &amount_raw,
                                                    const std::string &token,
                                                    const std::string &destination,
                                                    std::optional<std::int64_t> on_behalf_of)
    {
        nlohmann::json body = {{"amount", amount_raw}};
        if (!token.empty())
        {
            body["token"] = token;
        }
        if (!destination.empty())
        {
            body["destination"] = destination;
        }
        if (on_behalf_of)
        {
            body["onBehalfOf"] = *on_behalf_of;
        }
        return session_.request("POST", std::string(endpoints::portfolio_withdraw), body.dump());
    }

    Result<nlohmann::json> PositionClient::get_public_positions(const std::string &account)
    {
        return session_.get("/portfolio/" + detail::url_encode(account) + "/positions");
    }

    Result<nlohmann::json> PositionClient::get_public_traded_volume(const std::string &account)
    {
        return session_.get("/portfolio/" + detail::url_encode(account) + "/traded-volume");
    }

    Result<nlohmann::json> PositionClient::get_public_history(const std::string &account, const HistoryQuery &query)
    {
        detail::Query encoded;
        encoded.add("limit", std::to_string(query.limit));
        encoded.add_opt("cursor", query.cursor);
        encoded.add_opt("market", query.market);
        return session_.get("/portfolio/" + detail::url_encode(account) + "/history" + encoded.str());
    }

    Result<nlohmann::json> PositionClient::get_public_realized_pnl(const std::string &account,
                                                                   const std::string &timeframe)
    {
        detail::Query encoded;
        if (!timeframe.empty())
        {
            encoded.add("timeframe", timeframe);
        }
        return session_.get("/portfolio/" + detail::url_encode(account) + "/realized-pnl" + encoded.str());
    }
}
