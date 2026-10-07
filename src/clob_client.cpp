#include "limitless/clob_client.hpp"

#include "limitless/decimal_math.hpp"
#include "limitless/endpoints.hpp"
#include "query.hpp"

#include <stdexcept>

namespace limitless
{
    namespace
    {
        std::string require_one_of(const std::vector<std::string> &allowed, const std::string &value, const char *name)
        {
            for (const auto &candidate : allowed)
            {
                if (candidate == value)
                {
                    return value;
                }
            }
            throw std::invalid_argument(std::string(name) + " is not a documented value");
        }

        MarketSummary parse_market(const nlohmann::json &body)
        {
            if (!body.is_object() || !body.contains("slug") || !body["slug"].is_string())
            {
                throw std::invalid_argument("market payload is missing slug");
            }
            MarketSummary market;
            market.raw = body;
            market.slug = body["slug"].get<std::string>();
            if (body.contains("id") && body["id"].is_number())
            {
                market.id = body["id"].get<std::int64_t>();
            }
            if (body.contains("title") && body["title"].is_string())
            {
                market.title = body["title"].get<std::string>();
            }
            if (body.contains("status") && body["status"].is_string())
            {
                market.status = body["status"].get<std::string>();
            }
            if (body.contains("tradeType") && body["tradeType"].is_string())
            {
                market.trade_type = body["tradeType"].get<std::string>();
            }
            if (body.contains("marketType") && body["marketType"].is_string())
            {
                market.market_type = body["marketType"].get<std::string>();
            }
            if (body.contains("tokens") && body["tokens"].is_object())
            {
                TokenPair tokens;
                if (body["tokens"].contains("yes") && body["tokens"]["yes"].is_string())
                {
                    tokens.yes = body["tokens"]["yes"].get<std::string>();
                }
                if (body["tokens"].contains("no") && body["tokens"]["no"].is_string())
                {
                    tokens.no = body["tokens"]["no"].get<std::string>();
                }
                if (!tokens.yes.empty() || !tokens.no.empty())
                {
                    market.tokens = std::move(tokens);
                }
            }
            if (body.contains("venue") && body["venue"].is_object())
            {
                Venue venue;
                if (body["venue"].contains("exchange") && body["venue"]["exchange"].is_string())
                {
                    venue.exchange = body["venue"]["exchange"].get<std::string>();
                }
                if (body["venue"].contains("adapter") && body["venue"]["adapter"].is_string())
                {
                    venue.adapter = body["venue"]["adapter"].get<std::string>();
                }
                if (!venue.exchange.empty() || venue.adapter)
                {
                    market.venue = std::move(venue);
                }
            }
            if (body.contains("metadata") && body["metadata"].is_object() &&
                body["metadata"].contains("fee") && body["metadata"]["fee"].is_boolean())
            {
                market.fee = body["metadata"]["fee"].get<bool>();
            }
            return market;
        }

        ActiveMarketsPage parse_active_page(const nlohmann::json &body)
        {
            if (!body.is_object() || !body.contains("data") || !body["data"].is_array())
            {
                throw std::invalid_argument("active markets payload is missing data");
            }
            ActiveMarketsPage page;
            page.raw = body;
            if (body.contains("totalMarketsCount") && body["totalMarketsCount"].is_number())
            {
                page.total_markets_count = body["totalMarketsCount"].get<std::int64_t>();
            }
            for (const auto &item : body["data"])
            {
                if (item.is_object() && item.contains("slug"))
                {
                    page.markets.push_back(parse_market(item));
                }
            }
            return page;
        }

        Profile parse_profile(const nlohmann::json &body)
        {
            if (!body.is_object() || !body.contains("id") || !body["id"].is_number())
            {
                throw std::invalid_argument("profile payload is missing id");
            }
            Profile profile;
            profile.raw = body;
            profile.id = body["id"].get<std::int64_t>();
            if (body.contains("account") && body["account"].is_string())
            {
                profile.account = body["account"].get<std::string>();
            }
            if (body.contains("tradeWalletOption") && body["tradeWalletOption"].is_string())
            {
                profile.trade_wallet_option = body["tradeWalletOption"].get<std::string>();
            }
            if (body.contains("rank") && body["rank"].is_object() && body["rank"].contains("feeRateBps") &&
                body["rank"]["feeRateBps"].is_number())
            {
                profile.fee_rate_bps = body["rank"]["feeRateBps"].get<int>();
                profile.has_fee_rate = true;
            }
            return profile;
        }

        std::string active_query(const ActiveMarketsQuery &query)
        {
            detail::Query encoded;
            encoded.add_opt("page", query.page);
            encoded.add_opt("limit", query.limit);
            if (query.trade_type)
            {
                encoded.add("tradeType", require_one_of({"amm", "clob", "group"}, *query.trade_type, "tradeType"));
            }
            if (query.automation_type)
            {
                encoded.add("automationType",
                            require_one_of({"manual", "lumy", "sports"}, *query.automation_type, "automationType"));
            }
            encoded.add_bool("includeNextMarket", query.include_next_market);
            return encoded.str();
        }

        nlohmann::json amount_json(const std::string &raw)
        {
            if (raw.size() > 18)
            {
                throw std::invalid_argument("raw amount does not fit a JSON integer this client emits");
            }
            return nlohmann::json(std::stoull(raw));
        }

        void append_order_options(nlohmann::json &body,
                                  const std::optional<std::string> &client_order_id,
                                  const std::optional<std::int64_t> &on_behalf_of,
                                  const std::optional<std::string> &stp_policy,
                                  const std::optional<std::int64_t> &timestamp_ms,
                                  const std::optional<int> &recv_window_ms,
                                  bool post_only,
                                  bool include_post_only)
        {
            if (include_post_only && post_only)
            {
                body["postOnly"] = true;
            }
            if (client_order_id)
            {
                body["clientOrderId"] = *client_order_id;
            }
            if (on_behalf_of)
            {
                body["onBehalfOf"] = *on_behalf_of;
            }
            if (stp_policy)
            {
                body["stpPolicy"] = require_one_of({"cancel_maker", "cancel_taker", "cancel_both"},
                                                   *stp_policy,
                                                   "stpPolicy");
            }
            if (timestamp_ms)
            {
                body["timestamp"] = *timestamp_ms;
            }
            if (recv_window_ms)
            {
                if (*recv_window_ms < 1 || *recv_window_ms > 10000)
                {
                    throw std::invalid_argument("recvWindow must be from 1 through 10000");
                }
                body["recvWindow"] = *recv_window_ms;
            }
        }

        OrderDraft draft_from(const std::string &token_id,
                              const std::string &maker_amount,
                              const std::string &taker_amount,
                              Side side,
                              int fee_rate_bps,
                              std::uint8_t signature_type,
                              const std::string &maker,
                              const std::string &signer,
                              const std::string &taker,
                              const std::string &salt)
        {
            OrderDraft draft;
            draft.token_id = token_id;
            draft.maker_amount = maker_amount;
            draft.taker_amount = taker_amount;
            draft.side = side;
            draft.fee_rate_bps = static_cast<std::uint64_t>(fee_rate_bps);
            draft.signature_type = signature_type;
            draft.maker = maker;
            draft.signer = signer;
            draft.taker = taker.empty() ? kZeroAddress : taker;
            draft.salt = salt;
            return draft;
        }

    }

    ClobClient::ClobClient(Environment environment, NetworkOptions network)
        : session_(std::move(environment), std::move(network))
    {
    }

    void ClobClient::set_credentials(HmacCredentials credentials)
    {
        session_.set_credentials(std::move(credentials));
    }

    void ClobClient::clear_credentials()
    {
        session_.clear_credentials();
    }

    void ClobClient::set_signer(OrderSigner signer)
    {
        signer_ = std::move(signer);
    }

    const OrderSigner *ClobClient::signer() const
    {
        return signer_ ? &*signer_ : nullptr;
    }

    RestSession &ClobClient::session()
    {
        return session_;
    }

    const Environment &ClobClient::environment() const
    {
        return session_.environment();
    }

    Result<ActiveMarketsPage> ClobClient::get_active_markets(const ActiveMarketsQuery &query)
    {
        try
        {
            auto body = session_.get(std::string(endpoints::markets_active) + active_query(query));
            if (!body)
            {
                return Result<ActiveMarketsPage>::failure(body.error());
            }
            return Result<ActiveMarketsPage>::success(parse_active_page(body.value()));
        }
        catch (const std::invalid_argument &error)
        {
            return Result<ActiveMarketsPage>::failure(make_invalid_argument(error.what()));
        }
        catch (const std::exception &error)
        {
            return Result<ActiveMarketsPage>::failure(make_parse_error(error.what(), endpoints::markets_active.data()));
        }
    }

    Result<ActiveMarketsPage> ClobClient::get_active_markets_by_category(int category_id,
                                                                         const ActiveMarketsQuery &query)
    {
        const std::string path = std::string(endpoints::markets_active) + "/" + std::to_string(category_id) +
                                 active_query(query);
        try
        {
            auto body = session_.get(path);
            if (!body)
            {
                return Result<ActiveMarketsPage>::failure(body.error());
            }
            return Result<ActiveMarketsPage>::success(parse_active_page(body.value()));
        }
        catch (const std::invalid_argument &error)
        {
            return Result<ActiveMarketsPage>::failure(make_invalid_argument(error.what()));
        }
        catch (const std::exception &error)
        {
            return Result<ActiveMarketsPage>::failure(make_parse_error(error.what(), path));
        }
    }

    Result<nlohmann::json> ClobClient::get_active_slugs()
    {
        return session_.get(std::string(endpoints::markets_active_slugs));
    }

    Result<nlohmann::json> ClobClient::get_category_counts()
    {
        return session_.get(std::string(endpoints::markets_categories_count));
    }

    Result<nlohmann::json> ClobClient::search_markets(const SearchMarketsQuery &query)
    {
        if (query.query.empty())
        {
            return Result<nlohmann::json>::failure(make_invalid_argument("search query is required"));
        }
        detail::Query encoded;
        encoded.add("query", query.query);
        encoded.add_opt("limit", query.limit);
        encoded.add_opt("page", query.page);
        if (query.similarity_threshold)
        {
            encoded.add("similarityThreshold", std::to_string(*query.similarity_threshold));
        }
        encoded.add_bool("includeNextMarket", query.include_next_market);
        return session_.get(std::string(endpoints::markets_search) + encoded.str());
    }

    Result<MarketSummary> ClobClient::get_market(const std::string &address_or_slug, const std::string &include)
    {
        detail::Query encoded;
        if (!include.empty())
        {
            encoded.add("include", include);
        }
        const std::string path = "/markets/" + detail::url_encode(address_or_slug) + encoded.str();
        auto body = session_.get(path);
        if (!body)
        {
            return Result<MarketSummary>::failure(body.error());
        }
        try
        {
            const nlohmann::json *payload = &body.value();
            if (payload->is_object() && payload->contains("market") && (*payload)["market"].is_object() &&
                !payload->contains("slug"))
            {
                payload = &(*payload)["market"];
            }
            return Result<MarketSummary>::success(parse_market(*payload));
        }
        catch (const std::exception &error)
        {
            return Result<MarketSummary>::failure(make_parse_error(error.what(), path, body.value().dump()));
        }
    }

    Result<nlohmann::json> ClobClient::resolve_stable_slug(const std::string &slug)
    {
        return session_.get("/markets/stable/" + detail::url_encode(slug));
    }

    Result<OrderBook> ClobClient::get_orderbook(const std::string &slug)
    {
        const std::string path = "/markets/" + detail::url_encode(slug) + "/orderbook";
        auto body = session_.get(path);
        if (!body)
        {
            return Result<OrderBook>::failure(body.error());
        }
        try
        {
            auto book = OrderBook::parse_rest(body.value());
            book.market_slug = slug;
            return Result<OrderBook>::success(std::move(book));
        }
        catch (const std::exception &error)
        {
            return Result<OrderBook>::failure(make_parse_error(error.what(), path, body.value().dump()));
        }
    }

    Result<nlohmann::json> ClobClient::get_historical_price(const std::string &slug, const std::string &interval)
    {
        try
        {
            detail::Query encoded;
            if (!interval.empty())
            {
                encoded.add("interval",
                            require_one_of({"5m", "1h", "6h", "1d", "1w", "1m", "all"}, interval, "interval"));
            }
            return session_.get("/markets/" + detail::url_encode(slug) + "/historical-price" + encoded.str());
        }
        catch (const std::invalid_argument &error)
        {
            return Result<nlohmann::json>::failure(make_invalid_argument(error.what()));
        }
    }

    Result<nlohmann::json> ClobClient::get_market_events(const std::string &slug,
                                                         std::optional<int> page,
                                                         std::optional<int> limit)
    {
        detail::Query encoded;
        encoded.add_opt("page", page);
        encoded.add_opt("limit", limit);
        return session_.get("/markets/" + detail::url_encode(slug) + "/events" + encoded.str());
    }

    Result<nlohmann::json> ClobClient::get_feed_events(const std::string &slug,
                                                       std::optional<int> page,
                                                       std::optional<int> limit)
    {
        detail::Query encoded;
        encoded.add_opt("page", page);
        encoded.add_opt("limit", limit);
        return session_.get("/markets/" + detail::url_encode(slug) + "/get-feed-events" + encoded.str());
    }

    Result<nlohmann::json> ClobClient::get_timeline(const std::string &symbol,
                                                    const std::string &frequency,
                                                    const std::string &sub_frequency)
    {
        try
        {
            detail::Query encoded;
            encoded.add("symbol", symbol);
            encoded.add("frequency", require_one_of({"daily", "hourly", "minutely", "weekly"}, frequency, "frequency"));
            if (!sub_frequency.empty())
            {
                encoded.add("subFrequency",
                            require_one_of({"hours_1", "hours_2", "hours_3", "hours_4", "hours_5", "hours_6",
                                            "minutes_1", "minutes_5", "minutes_10", "minutes_15", "minutes_20",
                                            "minutes_25", "minutes_30", "minutes_35", "minutes_40", "minutes_45",
                                            "minutes_50", "minutes_55"},
                                           sub_frequency,
                                           "subFrequency"));
            }
            return session_.get(std::string(endpoints::markets_timeline) + encoded.str());
        }
        catch (const std::invalid_argument &error)
        {
            return Result<nlohmann::json>::failure(make_invalid_argument(error.what()));
        }
    }

    Result<nlohmann::json> ClobClient::get_market_timeline(const std::string &slug,
                                                           std::optional<int> before,
                                                           std::optional<int> after)
    {
        detail::Query encoded;
        encoded.add_opt("before", before);
        encoded.add_opt("after", after);
        return session_.get("/markets/" + detail::url_encode(slug) + "/timeline" + encoded.str());
    }

    Result<nlohmann::json> ClobClient::get_oracle_candles(const std::string &address_or_slug,
                                                          const std::string &interval,
                                                          std::optional<std::int64_t> from_unix,
                                                          std::optional<std::int64_t> to_unix)
    {
        try
        {
            detail::Query encoded;
            if (!interval.empty())
            {
                encoded.add("interval", require_one_of({"1m", "5m", "15m", "1h", "4h", "1d"}, interval, "interval"));
            }
            encoded.add_opt("from", from_unix);
            encoded.add_opt("to", to_unix);
            return session_.get("/markets/" + detail::url_encode(address_or_slug) + "/oracle-candles" + encoded.str());
        }
        catch (const std::invalid_argument &error)
        {
            return Result<nlohmann::json>::failure(make_invalid_argument(error.what()));
        }
    }

    Result<nlohmann::json> ClobClient::get_maintenance_status()
    {
        return session_.get(std::string(endpoints::maintenance_status));
    }

    Result<Profile> ClobClient::get_current_profile()
    {
        auto body = session_.get(std::string(endpoints::profiles_me));
        if (!body)
        {
            return Result<Profile>::failure(body.error());
        }
        try
        {
            return Result<Profile>::success(parse_profile(body.value()));
        }
        catch (const std::exception &error)
        {
            return Result<Profile>::failure(make_parse_error(error.what(), endpoints::profiles_me.data(), body.value().dump()));
        }
    }

    Result<Profile> ClobClient::get_profile(const std::string &account)
    {
        const std::string path = std::string(endpoints::profiles) + "/" + detail::url_encode(account);
        auto body = session_.get(path);
        if (!body)
        {
            return Result<Profile>::failure(body.error());
        }
        try
        {
            return Result<Profile>::success(parse_profile(body.value()));
        }
        catch (const std::exception &error)
        {
            return Result<Profile>::failure(make_parse_error(error.what(), path, body.value().dump()));
        }
    }

    Result<nlohmann::json> ClobClient::set_trade_wallet_option(const std::string &trade_wallet_option)
    {
        try
        {
            const auto option = require_one_of({"eoa", "smartWallet"}, trade_wallet_option, "tradeWalletOption");
            nlohmann::json body = {{"tradeWalletOption", option}};
            return session_.request("PUT", std::string(endpoints::profiles), body.dump());
        }
        catch (const std::invalid_argument &error)
        {
            return Result<nlohmann::json>::failure(make_invalid_argument(error.what()));
        }
    }

    Result<nlohmann::json> ClobClient::derive_api_token(const std::string &privy_identity_token,
                                                        const nlohmann::json &body)
    {
        if (privy_identity_token.empty())
        {
            return Result<nlohmann::json>::failure(make_invalid_argument("Privy identity token is required"));
        }
        return session_.request("POST",
                                std::string(endpoints::auth_api_tokens_derive),
                                body.dump(),
                                {{"identity", "Bearer " + privy_identity_token}, {"X-Limitless-Skip-Hmac", "1"}});
    }

    Result<nlohmann::json> ClobClient::get_user_orders(const std::string &slug, const UserOrdersQuery &query)
    {
        try
        {
            detail::Query encoded;
            for (const auto &status : query.statuses)
            {
                encoded.add("statuses",
                            require_one_of({"LIVE", "MATCHED", "CANCELED", "UNMATCHED"}, status, "statuses"));
            }
            encoded.add_opt("limit", query.limit);
            std::vector<std::pair<std::string, std::string>> headers;
            if (query.on_behalf_of)
            {
                headers.emplace_back("x-on-behalf-of", std::to_string(*query.on_behalf_of));
            }
            return session_.request("GET",
                                    "/markets/" + detail::url_encode(slug) + "/user-orders" + encoded.str(),
                                    {},
                                    std::move(headers));
        }
        catch (const std::invalid_argument &error)
        {
            return Result<nlohmann::json>::failure(make_invalid_argument(error.what()));
        }
    }

    PreparedOrder ClobClient::prepare_limit_order(const LimitOrderRequest &request) const
    {
        if (!signer_)
        {
            throw std::invalid_argument("order signer is not set");
        }
        if (request.type == OrderType::Fok)
        {
            throw std::invalid_argument("use prepare_market_order for FOK");
        }
        if (request.post_only && request.type != OrderType::Gtc)
        {
            throw std::invalid_argument("postOnly is only supported for GTC");
        }
        if (request.owner_id < 1)
        {
            throw std::invalid_argument("ownerId is required");
        }
        if (request.market_slug.empty() || request.token_id.empty() || request.verifying_contract.empty())
        {
            throw std::invalid_argument("marketSlug, tokenId, and verifying contract are required");
        }
        const auto amounts = limit_amounts(request.side, request.price, request.shares);
        auto signed_order = signer_->sign_order(draft_from(request.token_id,
                                                           amounts.maker_amount,
                                                           amounts.taker_amount,
                                                           request.side,
                                                           request.fee_rate_bps,
                                                           request.signature_type,
                                                           request.maker,
                                                           request.signer,
                                                           request.taker,
                                                           request.salt),
                                                 request.verifying_contract);
        nlohmann::json order = {{"salt", signed_order.order.salt},
                                {"maker", signed_order.order.maker},
                                {"signer", signed_order.order.signer},
                                {"taker", signed_order.order.taker},
                                {"tokenId", signed_order.order.token_id},
                                {"makerAmount", amount_json(signed_order.order.maker_amount)},
                                {"takerAmount", amount_json(signed_order.order.taker_amount)},
                                {"expiration", "0"},
                                {"nonce", 0},
                                {"feeRateBps", request.fee_rate_bps},
                                {"side", static_cast<int>(request.side)},
                                {"signatureType", signed_order.order.signature_type},
                                {"signature", signed_order.signature},
                                {"price", nlohmann::json::parse(amounts.price_text)}};
        nlohmann::json body = {{"order", std::move(order)},
                               {"ownerId", request.owner_id},
                               {"orderType", to_string(request.type)},
                               {"marketSlug", request.market_slug}};
        append_order_options(body,
                             request.client_order_id,
                             request.on_behalf_of,
                             request.stp_policy,
                             request.timestamp_ms,
                             request.recv_window_ms,
                             request.post_only,
                             true);
        PreparedOrder prepared;
        prepared.body = std::move(body);
        prepared.signature = signed_order.signature;
        prepared.digest = signed_order.digest;
        return prepared;
    }

    PreparedOrder ClobClient::prepare_market_order(const MarketOrderRequest &request) const
    {
        if (!signer_)
        {
            throw std::invalid_argument("order signer is not set");
        }
        if (request.owner_id < 1 || request.market_slug.empty() || request.token_id.empty() ||
            request.verifying_contract.empty())
        {
            throw std::invalid_argument("ownerId, marketSlug, tokenId, and verifying contract are required");
        }
        const auto amounts = fok_amounts(request.side, request.amount);
        auto signed_order = signer_->sign_order(draft_from(request.token_id,
                                                           amounts.maker_amount,
                                                           amounts.taker_amount,
                                                           request.side,
                                                           request.fee_rate_bps,
                                                           request.signature_type,
                                                           request.maker,
                                                           request.signer,
                                                           request.taker,
                                                           request.salt),
                                                 request.verifying_contract);
        nlohmann::json order = {{"salt", signed_order.order.salt},
                                {"maker", signed_order.order.maker},
                                {"signer", signed_order.order.signer},
                                {"taker", signed_order.order.taker},
                                {"tokenId", signed_order.order.token_id},
                                {"makerAmount", amount_json(signed_order.order.maker_amount)},
                                {"takerAmount", 1},
                                {"expiration", "0"},
                                {"nonce", 0},
                                {"feeRateBps", request.fee_rate_bps},
                                {"side", static_cast<int>(request.side)},
                                {"signatureType", signed_order.order.signature_type},
                                {"signature", signed_order.signature}};
        nlohmann::json body = {{"order", std::move(order)},
                               {"ownerId", request.owner_id},
                               {"orderType", "FOK"},
                               {"marketSlug", request.market_slug}};
        append_order_options(body,
                             request.client_order_id,
                             request.on_behalf_of,
                             request.stp_policy,
                             request.timestamp_ms,
                             request.recv_window_ms,
                             false,
                             false);
        PreparedOrder prepared;
        prepared.body = std::move(body);
        prepared.signature = std::move(signed_order.signature);
        prepared.digest = signed_order.digest;
        return prepared;
    }

    Result<nlohmann::json> ClobClient::create_order(const LimitOrderRequest &request)
    {
        try
        {
            const auto prepared = prepare_limit_order(request);
            return create_order_body(prepared.body, false);
        }
        catch (const std::invalid_argument &error)
        {
            return Result<nlohmann::json>::failure(make_invalid_argument(error.what()));
        }
        catch (const std::exception &error)
        {
            return Result<nlohmann::json>::failure(make_signing_error(error.what()));
        }
    }

    Result<nlohmann::json> ClobClient::create_market_order(const MarketOrderRequest &request)
    {
        try
        {
            const auto prepared = prepare_market_order(request);
            return create_order_body(prepared.body, false);
        }
        catch (const std::invalid_argument &error)
        {
            return Result<nlohmann::json>::failure(make_invalid_argument(error.what()));
        }
        catch (const std::exception &error)
        {
            return Result<nlohmann::json>::failure(make_signing_error(error.what()));
        }
    }

    Result<nlohmann::json> ClobClient::create_order_body(const nlohmann::json &body, bool async_place)
    {
        const auto path = async_place ? endpoints::orders_v2 : endpoints::orders;
        return session_.request("POST", std::string(path), body.dump());
    }

    Result<nlohmann::json> ClobClient::cancel_order(const std::string &order_id, std::optional<std::int64_t> on_behalf_of)
    {
        detail::Query encoded;
        encoded.add_opt("onBehalfOf", on_behalf_of);
        return session_.request("DELETE", "/orders/" + detail::url_encode(order_id) + encoded.str());
    }

    Result<nlohmann::json> ClobClient::cancel(const std::string &order_id,
                                              const std::string &client_order_id,
                                              std::optional<std::int64_t> on_behalf_of)
    {
        if (order_id.empty() == client_order_id.empty())
        {
            return Result<nlohmann::json>::failure(
                make_invalid_argument("provide exactly one of orderId or clientOrderId"));
        }
        nlohmann::json body = nlohmann::json::object();
        if (!order_id.empty())
        {
            body["orderId"] = order_id;
        }
        else
        {
            body["clientOrderId"] = client_order_id;
        }
        detail::Query encoded;
        encoded.add_opt("onBehalfOf", on_behalf_of);
        return session_.request("POST", std::string(endpoints::orders_cancel) + encoded.str(), body.dump());
    }

    Result<nlohmann::json> ClobClient::batch_cancel(const std::vector<std::string> &order_ids,
                                                    const std::vector<std::string> &client_order_ids,
                                                    std::optional<std::int64_t> on_behalf_of)
    {
        if (order_ids.empty() == client_order_ids.empty())
        {
            return Result<nlohmann::json>::failure(
                make_invalid_argument("provide exactly one of orderIds or clientOrderIds"));
        }
        nlohmann::json body = nlohmann::json::object();
        if (!order_ids.empty())
        {
            body["orderIds"] = order_ids;
        }
        else
        {
            body["clientOrderIds"] = client_order_ids;
        }
        detail::Query encoded;
        encoded.add_opt("onBehalfOf", on_behalf_of);
        return session_.request("POST", std::string(endpoints::orders_batch_cancel) + encoded.str(), body.dump());
    }

    Result<nlohmann::json> ClobClient::cancel_all(const std::string &slug, std::optional<std::int64_t> on_behalf_of)
    {
        detail::Query encoded;
        encoded.add_opt("onBehalfOf", on_behalf_of);
        return session_.request("DELETE", "/orders/all/" + detail::url_encode(slug) + encoded.str());
    }

    Result<nlohmann::json> ClobClient::cancel_replace(const nlohmann::json &body)
    {
        return session_.request("POST", std::string(endpoints::orders_cancel_replace), body.dump());
    }

    Result<nlohmann::json> ClobClient::cancel_replace_batch(const nlohmann::json &body)
    {
        return session_.request("POST", std::string(endpoints::orders_cancel_replace_batch), body.dump());
    }

    Result<nlohmann::json> ClobClient::order_status_batch(const nlohmann::json &body,
                                                          std::optional<std::int64_t> on_behalf_of)
    {
        std::vector<std::pair<std::string, std::string>> headers;
        if (on_behalf_of)
        {
            headers.emplace_back("x-on-behalf-of", std::to_string(*on_behalf_of));
        }
        return session_.request("POST",
                                std::string(endpoints::orders_status_batch),
                                body.dump(),
                                std::move(headers));
    }

    Result<nlohmann::json> ClobClient::get_async_order_status(const std::string &order_or_client_id)
    {
        return session_.get("/v2/orders/status/" + detail::url_encode(order_or_client_id));
    }

    Result<nlohmann::json> ClobClient::heartbeat(std::optional<std::int64_t> cancel_at_ms)
    {
        nlohmann::json body = nlohmann::json::object();
        if (cancel_at_ms)
        {
            body["cancelAt"] = *cancel_at_ms;
        }
        return session_.request("POST", std::string(endpoints::heartbeats), body.dump());
    }
}
