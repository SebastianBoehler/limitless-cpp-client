#include "limitless/clob_client.hpp"

#include "limitless/decimal_math.hpp"
#include "limitless/json_util.hpp"

#include <cctype>
#include <sstream>
#include <stdexcept>

namespace limitless {
namespace {

std::string url_encode(std::string_view value) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[c >> 4]);
            out.push_back(kHex[c & 0x0f]);
        }
    }
    return out;
}

std::string join_base(const std::string &base, const std::string &path) {
    std::string root = base;
    while (!root.empty() && root.back() == '/') root.pop_back();
    if (path.empty()) return root;
    if (path.front() == '/') return root + path;
    return root + "/" + path;
}

std::string excerpt(const std::string &body) {
    return body.size() <= 512 ? body : body.substr(0, 512);
}

std::string json_message(const std::string &body, long status) {
    try {
        const auto parsed = parse_json(body);
        if (parsed.is_object()) {
            if (parsed.contains("message") && parsed["message"].is_string()) {
                return parsed["message"].get<std::string>();
            }
            if (parsed.contains("error") && parsed["error"].is_string()) {
                return parsed["error"].get<std::string>();
            }
        }
    } catch (...) {
    }
    return "HTTP " + std::to_string(status);
}

SdkError http_error(const HttpResponse &response, const std::string &endpoint) {
    SdkError error;
    error.endpoint = endpoint;
    error.http_status = static_cast<int>(response.status);
    error.response_excerpt = excerpt(response.body);
    if (!response.transport_ok()) {
        error.code = SdkErrorCode::Transport;
        error.message = response.transport_error;
        error.retryable = true;
        return error;
    }
    if (response.status == 401 || response.status == 403) {
        error.code = SdkErrorCode::Auth;
    } else if (response.status == 429) {
        error.code = SdkErrorCode::RateLimit;
        error.retryable = true;
    } else if (response.status == 425) {
        error.code = SdkErrorCode::Unavailable;
        error.retryable = true;
    } else {
        error.code = SdkErrorCode::Api;
        error.retryable = response.status >= 500;
    }
    error.message = json_message(response.body, response.status);
    return error;
}

SdkError arg_error(std::string message) {
    SdkError error;
    error.code = SdkErrorCode::InvalidArgument;
    error.message = std::move(message);
    return error;
}

std::string append_query(std::string path, const std::string &query) {
    if (query.empty()) return path;
    path.push_back(path.find('?') == std::string::npos ? '?' : '&');
    path += query;
    return path;
}

MarketSummary parse_market(const nlohmann::json &input) {
    const nlohmann::json &json =
        input.contains("market") && input["market"].is_object() && !input.contains("slug") ? input["market"]
                                                                                            : input;
    MarketSummary market;
    if (json.contains("id") && json["id"].is_number_integer()) market.id = json["id"].get<std::int64_t>();
    market.slug = json.value("slug", "");
    market.title = json.value("title", "");
    market.status = json.value("status", "");
    market.trade_type = json.value("tradeType", "");
    market.market_type = json.value("marketType", "");
    if (json.contains("tokens") && json["tokens"].is_object()) {
        market.yes_token = json["tokens"].value("yes", "");
        market.no_token = json["tokens"].value("no", "");
    }
    if (json.contains("venue") && json["venue"].is_object()) {
        if (json["venue"].contains("exchange") && json["venue"]["exchange"].is_string()) {
            market.venue.exchange = json["venue"]["exchange"].get<std::string>();
        }
        if (json["venue"].contains("adapter") && json["venue"]["adapter"].is_string()) {
            market.venue.adapter = json["venue"]["adapter"].get<std::string>();
        }
    }
    if (json.contains("collateralToken") && json["collateralToken"].is_object()) {
        market.collateral.address = json["collateralToken"].value("address", "");
        market.collateral.decimals = json["collateralToken"].value("decimals", 6);
        market.collateral.symbol = json["collateralToken"].value("symbol", "");
    }
    if (json.contains("metadata") && json["metadata"].is_object()) {
        market.fee = json["metadata"].value("fee", false);
    }
    market.is_group = market.market_type == "group" || json.contains("markets");
    return market;
}

nlohmann::json int_or_string(const std::string &decimal) {
    try {
        const auto value = std::stoll(decimal);
        if (value >= 0 && value <= 9007199254740991LL) return value;
    } catch (...) {
    }
    return decimal;
}

nlohmann::json venue_json(const Venue &venue) {
    nlohmann::json json = nlohmann::json::object();
    if (!venue.exchange.empty()) json["exchange"] = venue.exchange;
    if (!venue.adapter.empty()) json["adapter"] = venue.adapter;
    return json;
}

}  // namespace

ClobClient::ClobClient(Environment env, HttpClientOptions http)
    : env_(std::move(env)), http_(std::move(http)) {}

ClobClient::~ClobClient() = default;

void ClobClient::set_credentials(HmacCredentials credentials) {
    credentials_ = std::move(credentials);
    have_credentials_ = !credentials_.token_id.empty() && !credentials_.secret_base64.empty();
}

Result<bool> ClobClient::set_signer(std::string private_key_hex) {
    auto signer = std::make_unique<OrderSigner>(std::move(private_key_hex));
    if (signer->address().empty()) {
        return Result<bool>::failure(arg_error("private key must be 32-byte hex"));
    }
    signer_ = std::move(signer);
    return Result<bool>::success(true);
}

const std::string &ClobClient::signer_address() const {
    static const std::string empty;
    return signer_ ? signer_->address() : empty;
}

const Venue *ClobClient::cached_venue(const std::string &slug) const {
    const auto it = venues_.find(slug);
    if (it == venues_.end()) return nullptr;
    return &it->second;
}

Result<nlohmann::json> ClobClient::request(
    std::string method, std::string path_and_query, std::string body, bool authenticate,
    std::vector<std::pair<std::string, std::string>> extra_headers) {
    HttpRequest http;
    http.method = std::move(method);
    http.url = join_base(env_.rest_url, path_and_query);
    http.body = std::move(body);
    http.headers = std::move(extra_headers);
    if (authenticate) {
        if (!have_credentials_) {
            return Result<nlohmann::json>::failure(arg_error("HMAC credentials are not set"));
        }
        try {
            const auto signed_headers =
                sign_request(credentials_, http.method, path_and_query, http.body);
            http.headers.insert(http.headers.end(), signed_headers.headers.begin(),
                                signed_headers.headers.end());
        } catch (const std::exception &ex) {
            SdkError error;
            error.code = SdkErrorCode::Signing;
            error.message = ex.what();
            error.endpoint = path_and_query;
            return Result<nlohmann::json>::failure(std::move(error));
        }
    }
    const HttpResponse response = http_.request(http);
    if (!response.ok()) return Result<nlohmann::json>::failure(http_error(response, path_and_query));
    if (response.body.empty()) return Result<nlohmann::json>::success(nlohmann::json(nullptr));
    try {
        return Result<nlohmann::json>::success(parse_json(response.body));
    } catch (const std::exception &ex) {
        SdkError error;
        error.code = SdkErrorCode::Parse;
        error.message = ex.what();
        error.endpoint = path_and_query;
        error.http_status = static_cast<int>(response.status);
        error.response_excerpt = excerpt(response.body);
        return Result<nlohmann::json>::failure(std::move(error));
    }
}

Result<nlohmann::json> ClobClient::authed(std::string method, std::string path, std::string body,
                                          std::vector<std::pair<std::string, std::string>> headers) {
    return request(std::move(method), std::move(path), std::move(body), true, std::move(headers));
}

std::string ClobClient::on_behalf_query(std::optional<std::int64_t> id) {
    if (!id) return {};
    return "onBehalfOf=" + std::to_string(*id);
}

Result<nlohmann::json> ClobClient::get_active_markets_json(const ActiveMarketsQuery &query) {
    std::string qs;
    auto add = [&](const std::string &key, const std::string &value) {
        if (value.empty()) return;
        if (!qs.empty()) qs.push_back('&');
        qs += url_encode(key) + "=" + url_encode(value);
    };
    if (query.page) add("page", std::to_string(*query.page));
    if (query.limit) add("limit", std::to_string(*query.limit));
    add("sortBy", query.sort_by);
    add("tradeType", query.trade_type);
    add("automationType", query.automation_type);
    if (query.include_next_market) add("includeNextMarket", *query.include_next_market ? "true" : "false");
    return request("GET", append_query("/markets/active", qs));
}

Result<ActiveMarketsPage> ClobClient::get_active_markets(const ActiveMarketsQuery &query) {
    auto json = get_active_markets_json(query);
    if (!json) return Result<ActiveMarketsPage>::failure(json.error());
    ActiveMarketsPage page;
    if (!json.value().is_object() || !json.value().contains("data") || !json.value()["data"].is_array()) {
        return Result<ActiveMarketsPage>::failure(arg_error("active markets response has no data array"));
    }
    page.total_markets_count = json.value().value("totalMarketsCount", 0.0);
    for (const auto &item : json.value()["data"]) page.data.push_back(parse_market(item));
    return Result<ActiveMarketsPage>::success(std::move(page));
}

Result<nlohmann::json> ClobClient::get_market_json(const std::string &slug_or_address) {
    return request("GET", "/markets/" + url_encode(slug_or_address));
}

Result<MarketSummary> ClobClient::get_market(const std::string &slug_or_address) {
    return load_market(slug_or_address);
}

Result<MarketSummary> ClobClient::load_market(const std::string &slug) {
    auto json = get_market_json(slug);
    if (!json) return Result<MarketSummary>::failure(json.error());
    MarketSummary market = parse_market(json.value());
    const std::string key = market.slug.empty() ? slug : market.slug;
    markets_[key] = market;
    if (!market.venue.exchange.empty()) venues_[key] = market.venue;
    if (!slug.empty() && slug != key) {
        markets_[slug] = market;
        if (!market.venue.exchange.empty()) venues_[slug] = market.venue;
    }
    return Result<MarketSummary>::success(std::move(market));
}

Result<nlohmann::json> ClobClient::search_markets(const std::string &query, int limit, int page,
                                                  double similarity_threshold) {
    std::ostringstream qs;
    qs << "query=" << url_encode(query) << "&limit=" << limit << "&page=" << page
       << "&similarityThreshold=" << similarity_threshold;
    return request("GET", "/markets/search?" + qs.str());
}

Result<Orderbook> ClobClient::get_orderbook(const std::string &slug) {
    auto json = request("GET", "/markets/" + url_encode(slug) + "/orderbook");
    if (!json) return Result<Orderbook>::failure(json.error());
    return Result<Orderbook>::success(parse_orderbook(json.value()));
}

Result<nlohmann::json> ClobClient::get_historical_prices(const std::string &slug,
                                                         const std::string &interval) {
    return request("GET", "/markets/" + url_encode(slug) + "/historical-price?interval=" + url_encode(interval));
}

Result<nlohmann::json> ClobClient::get_market_events(const std::string &slug, int page, int limit) {
    return request("GET", "/markets/" + url_encode(slug) + "/events?page=" + std::to_string(page) +
                              "&limit=" + std::to_string(limit));
}

Result<nlohmann::json> ClobClient::get_active_slugs() { return request("GET", "/markets/active/slugs"); }

Result<nlohmann::json> ClobClient::get_category_counts() {
    return request("GET", "/markets/categories/count");
}

Result<nlohmann::json> ClobClient::get_maintenance_status() {
    return request("GET", "/maintenance/status");
}

Result<nlohmann::json> ClobClient::get_profile() { return authed("GET", "/profiles/me"); }

Result<nlohmann::json> ClobClient::set_trade_wallet_option(const std::string &option) {
    if (option != "eoa" && option != "smartWallet") {
        return Result<nlohmann::json>::failure(
            arg_error("tradeWalletOption must be \"eoa\" or \"smartWallet\""));
    }
    return authed("PUT", "/profiles", nlohmann::json({{"tradeWalletOption", option}}).dump());
}

Result<nlohmann::json> ClobClient::get_positions(std::optional<std::int64_t> on_behalf_of) {
    std::vector<std::pair<std::string, std::string>> headers;
    if (on_behalf_of) headers.emplace_back("x-on-behalf-of", std::to_string(*on_behalf_of));
    return authed("GET", "/portfolio/positions", {}, std::move(headers));
}

Result<nlohmann::json> ClobClient::get_trades() { return authed("GET", "/portfolio/trades"); }

Result<nlohmann::json> ClobClient::get_history(int limit, const std::string &market,
                                               const std::string &cursor,
                                               std::optional<std::int64_t> on_behalf_of) {
    std::string qs = "limit=" + std::to_string(limit);
    if (!market.empty()) qs += "&market=" + url_encode(market);
    if (!cursor.empty()) qs += "&cursor=" + url_encode(cursor);
    std::vector<std::pair<std::string, std::string>> headers;
    if (on_behalf_of) headers.emplace_back("x-on-behalf-of", std::to_string(*on_behalf_of));
    return authed("GET", "/portfolio/history?" + qs, {}, std::move(headers));
}

Result<nlohmann::json> ClobClient::get_user_orders(const std::string &slug,
                                                   const std::vector<std::string> &statuses,
                                                   std::optional<int> limit,
                                                   std::optional<std::int64_t> on_behalf_of) {
    std::string qs;
    for (const auto &status : statuses) {
        if (!qs.empty()) qs.push_back('&');
        qs += "statuses=" + url_encode(status);
    }
    if (limit) {
        if (!qs.empty()) qs.push_back('&');
        qs += "limit=" + std::to_string(*limit);
    }
    std::vector<std::pair<std::string, std::string>> headers;
    if (on_behalf_of) headers.emplace_back("x-on-behalf-of", std::to_string(*on_behalf_of));
    return authed("GET", append_query("/markets/" + url_encode(slug) + "/user-orders", qs), {},
                  std::move(headers));
}

Result<SignedOrder> ClobClient::create_order(const OrderRequest &request) {
    if (request.market_slug.empty()) {
        return Result<SignedOrder>::failure(arg_error("market_slug is required"));
    }
    if (request.post_only && request.type != OrderType::Gtc) {
        return Result<SignedOrder>::failure(arg_error("postOnly is supported only for GTC orders"));
    }
    auto market = load_market(request.market_slug);
    if (!market) return Result<SignedOrder>::failure(market.error());
    if (market.value().venue.exchange.empty()) {
        return Result<SignedOrder>::failure(arg_error("market has no venue.exchange"));
    }

    std::string token_id = request.token_id;
    if (token_id.empty()) {
        token_id = request.outcome == OutcomeToken::Yes ? market.value().yes_token : market.value().no_token;
    }
    if (token_id.empty()) return Result<SignedOrder>::failure(arg_error("token id is missing"));

    int fee_bps = 0;
    std::int64_t owner_id = request.owner_id.value_or(0);
    if (!request.fee_rate_bps || owner_id == 0) {
        if (market.value().fee || owner_id == 0) {
            if (!request.fee_rate_bps && market.value().fee && !have_credentials_) {
                return Result<SignedOrder>::failure(arg_error(
                    "fee market requires fee_rate_bps from GET /profiles/me rank.feeRateBps"));
            }
            if (have_credentials_ && (!request.fee_rate_bps || owner_id == 0)) {
                auto profile = get_profile();
                if (!profile) return Result<SignedOrder>::failure(profile.error());
                if (owner_id == 0) {
                    if (!profile.value().contains("id") || !profile.value()["id"].is_number_integer()) {
                        return Result<SignedOrder>::failure(arg_error("profile response has no id"));
                    }
                    owner_id = profile.value()["id"].get<std::int64_t>();
                }
                if (!request.fee_rate_bps) {
                    if (market.value().fee) {
                        if (!profile.value().contains("rank") || !profile.value()["rank"].is_object() ||
                            !profile.value()["rank"].contains("feeRateBps")) {
                            return Result<SignedOrder>::failure(
                                arg_error("profile response has no rank.feeRateBps"));
                        }
                        fee_bps = profile.value()["rank"]["feeRateBps"].get<int>();
                    }
                }
            }
        }
    }
    if (request.fee_rate_bps) fee_bps = *request.fee_rate_bps;
    if (owner_id == 0 && !request.delegated) {
        return Result<SignedOrder>::failure(arg_error("ownerId is required (profile id from GET /profiles/me)"));
    }

    ScaledAmounts amounts;
    std::string amount_error;
    if (request.type == OrderType::Fok) {
        const std::string human = request.side == OrderSide::Buy
                                      ? (request.maker_amount.empty() ? request.size : request.maker_amount)
                                      : (request.size.empty() ? request.maker_amount : request.size);
        if (!fok_amounts(request.side, human, amounts, amount_error)) {
            return Result<SignedOrder>::failure(arg_error(amount_error));
        }
    } else if (!gtc_amounts(request.side, request.price, request.size, amounts, amount_error)) {
        return Result<SignedOrder>::failure(arg_error(amount_error));
    }

    UnsignedOrder order;
    order.salt = next_order_salt();
    order.maker = request.maker.empty() ? signer_address() : request.maker;
    order.signer = request.signer.empty() ? order.maker : request.signer;
    order.token_id = token_id;
    order.maker_amount = amounts.maker_amount;
    order.taker_amount = amounts.taker_amount;
    order.fee_rate_bps = std::to_string(fee_bps);
    order.side = static_cast<std::uint8_t>(request.side);
    order.signature_type = 0;
    if (order.maker.empty() || order.signer.empty()) {
        return Result<SignedOrder>::failure(arg_error("maker and signer addresses are required"));
    }
    order.maker = to_checksum_address(order.maker);
    order.signer = to_checksum_address(order.signer);

    if (request.delegated) {
        SignedOrder delegated;
        delegated.order = std::move(order);
        delegated.price = amounts.price;
        return Result<SignedOrder>::success(std::move(delegated));
    }
    if (!signer_) return Result<SignedOrder>::failure(arg_error("order signer is not set"));
    return signer_->sign_order(env_.order_domain(market.value().venue.exchange), std::move(order),
                               amounts.price);
}

Result<nlohmann::json> ClobClient::post_signed_order(const SignedOrder &order, const OrderRequest &request) {
    if (request.owner_id.value_or(0) == 0 && !have_credentials_) {
        return Result<nlohmann::json>::failure(arg_error("ownerId is required"));
    }
    std::int64_t owner_id = request.owner_id.value_or(0);
    if (owner_id == 0) {
        auto profile = get_profile();
        if (!profile) return profile;
        owner_id = profile.value().value("id", static_cast<std::int64_t>(0));
        if (owner_id == 0) return Result<nlohmann::json>::failure(arg_error("profile response has no id"));
    }

    nlohmann::json wire = {
        {"salt", order.order.salt},
        {"maker", order.order.maker},
        {"signer", order.order.signer},
        {"taker", order.order.taker},
        {"tokenId", order.order.token_id},
        {"makerAmount", int_or_string(order.order.maker_amount)},
        {"takerAmount", int_or_string(order.order.taker_amount)},
        {"expiration", "0"},
        {"nonce", 0},
        {"feeRateBps", int_or_string(order.order.fee_rate_bps)},
        {"side", order.order.side},
    };
    if (!order.price.empty()) wire["price"] = nlohmann::json::parse(order.price);
    if (!request.delegated) {
        wire["signature"] = order.signature;
        wire["signatureType"] = order.order.signature_type;
    }
    nlohmann::json body = {{"order", std::move(wire)},
                           {"ownerId", owner_id},
                           {"orderType", order_type_wire(request.type)},
                           {"marketSlug", request.market_slug}};
    if (request.post_only) body["postOnly"] = true;
    if (!request.client_order_id.empty()) body["clientOrderId"] = request.client_order_id;
    if (request.on_behalf_of) body["onBehalfOf"] = *request.on_behalf_of;
    if (request.timestamp_ms) body["timestamp"] = *request.timestamp_ms;
    if (request.recv_window_ms) body["recvWindow"] = *request.recv_window_ms;
    if (!request.stp_policy.empty()) body["stpPolicy"] = request.stp_policy;
    const std::string path = request.async_submit ? "/v2/orders" : "/orders";
    return authed("POST", path, body.dump());
}

Result<nlohmann::json> ClobClient::create_and_post_order(const OrderRequest &request) {
    auto signed_order = create_order(request);
    if (!signed_order) return Result<nlohmann::json>::failure(signed_order.error());
    OrderRequest posted = request;
    if (!posted.owner_id) {
        auto profile = have_credentials_ ? get_profile() : Result<nlohmann::json>::failure(arg_error("ownerId is required"));
        if (!profile) return profile;
        posted.owner_id = profile.value().value("id", static_cast<std::int64_t>(0));
    }
    return post_signed_order(signed_order.value(), posted);
}

Result<nlohmann::json> ClobClient::cancel_order(const std::string &order_id,
                                                std::optional<std::int64_t> on_behalf_of) {
    return authed("DELETE", append_query("/orders/" + url_encode(order_id), on_behalf_query(on_behalf_of)));
}

Result<nlohmann::json> ClobClient::cancel_by_client_id(const std::string &client_order_id,
                                                       std::optional<std::int64_t> on_behalf_of) {
    const nlohmann::json body = {{"clientOrderId", client_order_id}};
    return authed("POST", append_query("/orders/cancel", on_behalf_query(on_behalf_of)), body.dump());
}

Result<nlohmann::json> ClobClient::cancel_orders(const std::vector<std::string> &order_ids,
                                                 std::optional<std::int64_t> on_behalf_of) {
    return authed("POST", append_query("/orders/batch-cancel", on_behalf_query(on_behalf_of)),
                  nlohmann::json({{"orderIds", order_ids}}).dump());
}

Result<nlohmann::json> ClobClient::cancel_client_orders(const std::vector<std::string> &client_order_ids,
                                                        std::optional<std::int64_t> on_behalf_of) {
    return authed("POST", append_query("/orders/batch-cancel", on_behalf_query(on_behalf_of)),
                  nlohmann::json({{"clientOrderIds", client_order_ids}}).dump());
}

Result<nlohmann::json> ClobClient::cancel_all(const std::string &slug,
                                              std::optional<std::int64_t> on_behalf_of) {
    return authed("DELETE", append_query("/orders/all/" + url_encode(slug), on_behalf_query(on_behalf_of)));
}

Result<nlohmann::json> ClobClient::order_status_batch(const nlohmann::json &body,
                                                      std::optional<std::int64_t> on_behalf_of) {
    std::vector<std::pair<std::string, std::string>> headers;
    if (on_behalf_of) headers.emplace_back("x-on-behalf-of", std::to_string(*on_behalf_of));
    return authed("POST", "/orders/status/batch", body.dump(), std::move(headers));
}

Result<nlohmann::json> ClobClient::set_heartbeat(std::optional<std::int64_t> cancel_at_ms) {
    nlohmann::json body = nlohmann::json::object();
    if (cancel_at_ms) body["cancelAt"] = *cancel_at_ms;
    return authed("POST", "/heartbeats", body.dump());
}

Result<nlohmann::json> ClobClient::split_positions(const std::string &condition_id, const std::string &amount,
                                                   const Venue &venue, std::optional<std::int64_t> on_behalf_of) {
    nlohmann::json body = {{"conditionId", condition_id}, {"amount", amount}, {"venue", venue_json(venue)}};
    if (on_behalf_of) body["onBehalfOf"] = *on_behalf_of;
    return authed("POST", "/portfolio/split", body.dump());
}

Result<nlohmann::json> ClobClient::merge_positions(const std::string &condition_id, const std::string &amount,
                                                   const Venue &venue, std::optional<std::int64_t> on_behalf_of) {
    nlohmann::json body = {{"conditionId", condition_id}, {"amount", amount}, {"venue", venue_json(venue)}};
    if (on_behalf_of) body["onBehalfOf"] = *on_behalf_of;
    return authed("POST", "/portfolio/merge", body.dump());
}

Result<nlohmann::json> ClobClient::redeem_positions(const std::string &condition_id,
                                                    std::optional<std::int64_t> on_behalf_of) {
    nlohmann::json body = {{"conditionId", condition_id}};
    if (on_behalf_of) body["onBehalfOf"] = *on_behalf_of;
    return authed("POST", "/portfolio/redeem", body.dump());
}

Result<bool> ClobClient::warm_connection() {
    ActiveMarketsQuery warm;
    warm.limit = 1;
    auto page = get_active_markets_json(warm);
    if (!page) return Result<bool>::failure(page.error());
    return Result<bool>::success(true);
}

}  // namespace limitless
