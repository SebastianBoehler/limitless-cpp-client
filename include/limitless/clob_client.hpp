#pragma once

#include "limitless/environment.hpp"
#include "limitless/hmac_auth.hpp"
#include "limitless/http_client.hpp"
#include "limitless/order_signer.hpp"
#include "limitless/orderbook.hpp"
#include "limitless/types.hpp"

#include <nlohmann/json.hpp>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace limitless {

class ClobClient {
public:
    explicit ClobClient(Environment env = Environment::production(),
                        HttpClientOptions http = {});
    ~ClobClient();

    ClobClient(const ClobClient &) = delete;
    ClobClient &operator=(const ClobClient &) = delete;

    void set_credentials(HmacCredentials credentials);
    Result<bool> set_signer(std::string private_key_hex);
    const Environment &environment() const { return env_; }
    const std::string &signer_address() const;

    Result<nlohmann::json> request(std::string method, std::string path_and_query,
                                   std::string body = {}, bool authenticate = false,
                                   std::vector<std::pair<std::string, std::string>> extra_headers = {});

    Result<ActiveMarketsPage> get_active_markets(const ActiveMarketsQuery &query = {});
    Result<nlohmann::json> get_active_markets_json(const ActiveMarketsQuery &query = {});
    Result<MarketSummary> get_market(const std::string &slug_or_address);
    Result<nlohmann::json> get_market_json(const std::string &slug_or_address);
    Result<nlohmann::json> search_markets(const std::string &query, int limit = 10, int page = 1,
                                          double similarity_threshold = 0.5);
    Result<Orderbook> get_orderbook(const std::string &slug);
    Result<nlohmann::json> get_historical_prices(const std::string &slug,
                                                 const std::string &interval = "1d");
    Result<nlohmann::json> get_market_events(const std::string &slug, int page = 1, int limit = 100);
    Result<nlohmann::json> get_active_slugs();
    Result<nlohmann::json> get_category_counts();
    Result<nlohmann::json> get_maintenance_status();

    Result<nlohmann::json> get_profile();
    Result<nlohmann::json> set_trade_wallet_option(const std::string &option);
    Result<nlohmann::json> get_positions(std::optional<std::int64_t> on_behalf_of = std::nullopt);
    Result<nlohmann::json> get_trades();
    Result<nlohmann::json> get_history(int limit, const std::string &market = {},
                                       const std::string &cursor = {},
                                       std::optional<std::int64_t> on_behalf_of = std::nullopt);
    Result<nlohmann::json> get_user_orders(const std::string &slug,
                                           const std::vector<std::string> &statuses = {},
                                           std::optional<int> limit = std::nullopt,
                                           std::optional<std::int64_t> on_behalf_of = std::nullopt);

    Result<SignedOrder> create_order(const OrderRequest &request);
    Result<nlohmann::json> post_signed_order(const SignedOrder &order, const OrderRequest &request);
    Result<nlohmann::json> create_and_post_order(const OrderRequest &request);

    Result<nlohmann::json> cancel_order(const std::string &order_id,
                                        std::optional<std::int64_t> on_behalf_of = std::nullopt);
    Result<nlohmann::json> cancel_by_client_id(const std::string &client_order_id,
                                               std::optional<std::int64_t> on_behalf_of = std::nullopt);
    Result<nlohmann::json> cancel_orders(const std::vector<std::string> &order_ids,
                                         std::optional<std::int64_t> on_behalf_of = std::nullopt);
    Result<nlohmann::json> cancel_client_orders(const std::vector<std::string> &client_order_ids,
                                                std::optional<std::int64_t> on_behalf_of = std::nullopt);
    Result<nlohmann::json> cancel_all(const std::string &slug,
                                      std::optional<std::int64_t> on_behalf_of = std::nullopt);
    Result<nlohmann::json> order_status_batch(const nlohmann::json &body,
                                              std::optional<std::int64_t> on_behalf_of = std::nullopt);
    Result<nlohmann::json> set_heartbeat(std::optional<std::int64_t> cancel_at_ms);

    Result<nlohmann::json> split_positions(const std::string &condition_id, const std::string &amount,
                                           const Venue &venue,
                                           std::optional<std::int64_t> on_behalf_of = std::nullopt);
    Result<nlohmann::json> merge_positions(const std::string &condition_id, const std::string &amount,
                                           const Venue &venue,
                                           std::optional<std::int64_t> on_behalf_of = std::nullopt);
    Result<nlohmann::json> redeem_positions(const std::string &condition_id,
                                            std::optional<std::int64_t> on_behalf_of = std::nullopt);

    Result<bool> warm_connection();
    const Venue *cached_venue(const std::string &slug) const;

private:
    Environment env_;
    HttpClient http_;
    HmacCredentials credentials_;
    bool have_credentials_ = false;
    std::unique_ptr<OrderSigner> signer_;
    std::unordered_map<std::string, Venue> venues_;
    std::unordered_map<std::string, MarketSummary> markets_;

    Result<MarketSummary> load_market(const std::string &slug);
    Result<nlohmann::json> authed(std::string method, std::string path, std::string body = {},
                                  std::vector<std::pair<std::string, std::string>> headers = {});
    static std::string on_behalf_query(std::optional<std::int64_t> id);
};

}  // namespace limitless
