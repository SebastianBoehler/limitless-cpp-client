#pragma once

#include "limitless/order_signer.hpp"
#include "limitless/orderbook.hpp"
#include "limitless/rest_session.hpp"
#include "limitless/types.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace limitless
{
    struct LimitOrderRequest
    {
        std::string market_slug;
        std::string token_id;
        std::string verifying_contract;
        Side side{Side::Buy};
        OrderType type{OrderType::Gtc};
        std::string price;
        std::string shares;
        std::int64_t owner_id{0};
        int fee_rate_bps{0};
        bool post_only{false};
        std::optional<std::string> client_order_id;
        std::optional<std::int64_t> on_behalf_of;
        std::optional<std::string> stp_policy;
        std::optional<std::int64_t> timestamp_ms;
        std::optional<int> recv_window_ms;
        std::uint8_t signature_type{0};
        std::string maker;
        std::string signer;
        std::string taker;
        std::string salt;
    };

    struct MarketOrderRequest
    {
        std::string market_slug;
        std::string token_id;
        std::string verifying_contract;
        Side side{Side::Buy};
        // BUY: USDC to spend. SELL: shares to sell. Human units, scaled by 1e6.
        std::string amount;
        std::int64_t owner_id{0};
        int fee_rate_bps{0};
        std::optional<std::string> client_order_id;
        std::optional<std::int64_t> on_behalf_of;
        std::optional<std::string> stp_policy;
        std::optional<std::int64_t> timestamp_ms;
        std::optional<int> recv_window_ms;
        std::uint8_t signature_type{0};
        std::string maker;
        std::string signer;
        std::string taker;
        std::string salt;
    };

    struct PreparedOrder
    {
        nlohmann::json body;
        std::string signature;
        std::array<std::uint8_t, 32> digest{};
    };

    class ClobClient
    {
    public:
        explicit ClobClient(Environment environment = Environment::base(),
                            NetworkOptions network = {});

        void set_credentials(HmacCredentials credentials);
        void clear_credentials();
        void set_signer(OrderSigner signer);
        const OrderSigner *signer() const;
        RestSession &session();
        const Environment &environment() const;

        Result<ActiveMarketsPage> get_active_markets(const ActiveMarketsQuery &query = {});
        Result<ActiveMarketsPage> get_active_markets_by_category(int category_id,
                                                                 const ActiveMarketsQuery &query = {});
        Result<nlohmann::json> get_active_slugs();
        Result<nlohmann::json> get_category_counts();
        Result<nlohmann::json> search_markets(const SearchMarketsQuery &query);
        Result<MarketSummary> get_market(const std::string &address_or_slug,
                                         const std::string &include = {});
        Result<nlohmann::json> resolve_stable_slug(const std::string &slug);
        Result<OrderBook> get_orderbook(const std::string &slug);
        Result<nlohmann::json> get_historical_price(const std::string &slug,
                                                    const std::string &interval = "1d");
        Result<nlohmann::json> get_market_events(const std::string &slug,
                                                 std::optional<int> page = {},
                                                 std::optional<int> limit = {});
        Result<nlohmann::json> get_feed_events(const std::string &slug,
                                               std::optional<int> page = {},
                                               std::optional<int> limit = {});
        Result<nlohmann::json> get_timeline(const std::string &symbol,
                                            const std::string &frequency,
                                            const std::string &sub_frequency = {});
        Result<nlohmann::json> get_market_timeline(const std::string &slug,
                                                   std::optional<int> before = {},
                                                   std::optional<int> after = {});
        Result<nlohmann::json> get_oracle_candles(const std::string &address_or_slug,
                                                  const std::string &interval = {},
                                                  std::optional<std::int64_t> from_unix = {},
                                                  std::optional<std::int64_t> to_unix = {});
        Result<nlohmann::json> get_maintenance_status();

        Result<Profile> get_current_profile();
        Result<Profile> get_profile(const std::string &account);
        Result<nlohmann::json> set_trade_wallet_option(const std::string &trade_wallet_option);

        // Privy identity token in the `identity: Bearer ...` header. Not an HMAC call.
        Result<nlohmann::json> derive_api_token(const std::string &privy_identity_token,
                                                const nlohmann::json &body = nlohmann::json::object());

        Result<nlohmann::json> get_user_orders(const std::string &slug, const UserOrdersQuery &query = {});

        PreparedOrder prepare_limit_order(const LimitOrderRequest &request) const;
        PreparedOrder prepare_market_order(const MarketOrderRequest &request) const;

        Result<nlohmann::json> create_order(const LimitOrderRequest &request);
        Result<nlohmann::json> create_market_order(const MarketOrderRequest &request);
        Result<nlohmann::json> create_order_body(const nlohmann::json &body, bool async_place = false);

        Result<nlohmann::json> cancel_order(const std::string &order_id,
                                            std::optional<std::int64_t> on_behalf_of = {});
        Result<nlohmann::json> cancel(const std::string &order_id,
                                      const std::string &client_order_id,
                                      std::optional<std::int64_t> on_behalf_of = {});
        Result<nlohmann::json> batch_cancel(const std::vector<std::string> &order_ids,
                                            const std::vector<std::string> &client_order_ids,
                                            std::optional<std::int64_t> on_behalf_of = {});
        Result<nlohmann::json> cancel_all(const std::string &slug,
                                          std::optional<std::int64_t> on_behalf_of = {});
        Result<nlohmann::json> cancel_replace(const nlohmann::json &body);
        Result<nlohmann::json> cancel_replace_batch(const nlohmann::json &body);
        Result<nlohmann::json> order_status_batch(const nlohmann::json &body,
                                                  std::optional<std::int64_t> on_behalf_of = {});
        Result<nlohmann::json> get_async_order_status(const std::string &order_or_client_id);
        Result<nlohmann::json> heartbeat(std::optional<std::int64_t> cancel_at_ms = {});

    private:
        RestSession session_;
        std::optional<OrderSigner> signer_;
    };
}
