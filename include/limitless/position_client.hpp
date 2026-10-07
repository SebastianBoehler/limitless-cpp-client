#pragma once

#include "limitless/rest_session.hpp"
#include "limitless/types.hpp"

#include <optional>
#include <string>

namespace limitless
{
    struct PositionOperation
    {
        std::string condition_id;
        std::string amount_raw;
        Venue venue;
        std::optional<std::int64_t> on_behalf_of;
    };

    class PositionClient
    {
    public:
        explicit PositionClient(Environment environment = Environment::base(),
                                NetworkOptions network = {});

        void set_credentials(HmacCredentials credentials);
        RestSession &session();

        Result<nlohmann::json> get_positions(std::optional<std::int64_t> on_behalf_of = {});
        Result<nlohmann::json> get_trades();
        Result<nlohmann::json> get_history(const HistoryQuery &query);
        Result<nlohmann::json> get_pnl_chart(const std::string &timeframe = {});
        Result<nlohmann::json> get_points();
        Result<nlohmann::json> get_allowance(AllowanceType type, const std::string &spender = {});
        Result<nlohmann::json> redeem(const std::string &condition_id,
                                      std::optional<std::int64_t> on_behalf_of = {});
        Result<nlohmann::json> split(const PositionOperation &operation);
        Result<nlohmann::json> merge(const PositionOperation &operation);
        Result<nlohmann::json> withdraw(const std::string &amount_raw,
                                        const std::string &token = {},
                                        const std::string &destination = {},
                                        std::optional<std::int64_t> on_behalf_of = {});

        Result<nlohmann::json> get_public_positions(const std::string &account);
        Result<nlohmann::json> get_public_traded_volume(const std::string &account);
        Result<nlohmann::json> get_public_history(const std::string &account, const HistoryQuery &query);
        Result<nlohmann::json> get_public_realized_pnl(const std::string &account,
                                                       const std::string &timeframe = {});

    private:
        RestSession session_;
    };
}
