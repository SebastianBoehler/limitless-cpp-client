#pragma once

#include <string_view>

namespace limitless::endpoints
{
    // Path templates from https://api.limitless.exchange/api-json (fetched 2026-10-07)
    // plus routes whose docs pages and live probes agree they exist:
    //   GET /maintenance/status (200)
    //   GET /profiles/me, GET /profiles/{account}, PUT /profiles (401 without auth)
    //   POST /auth/api-tokens/derive (401 without a Privy identity token)

    inline constexpr std::string_view markets_active = "/markets/active";
    inline constexpr std::string_view markets_active_slugs = "/markets/active/slugs";
    inline constexpr std::string_view markets_categories_count = "/markets/categories/count";
    inline constexpr std::string_view markets_search = "/markets/search";
    inline constexpr std::string_view markets_timeline = "/markets/timeline";
    inline constexpr std::string_view orders = "/orders";
    inline constexpr std::string_view orders_v2 = "/v2/orders";
    inline constexpr std::string_view orders_cancel = "/orders/cancel";
    inline constexpr std::string_view orders_batch_cancel = "/orders/batch-cancel";
    inline constexpr std::string_view orders_cancel_batch = "/orders/cancel-batch";
    inline constexpr std::string_view orders_cancel_replace = "/orders/cancel-replace";
    inline constexpr std::string_view orders_cancel_replace_batch = "/orders/cancel-replace/batch";
    inline constexpr std::string_view orders_status_batch = "/orders/status/batch";
    inline constexpr std::string_view heartbeats = "/heartbeats";
    inline constexpr std::string_view portfolio_positions = "/portfolio/positions";
    inline constexpr std::string_view portfolio_trades = "/portfolio/trades";
    inline constexpr std::string_view portfolio_history = "/portfolio/history";
    inline constexpr std::string_view portfolio_pnl_chart = "/portfolio/pnl-chart";
    inline constexpr std::string_view portfolio_points = "/portfolio/points";
    inline constexpr std::string_view portfolio_allowance = "/portfolio/trading/allowance";
    inline constexpr std::string_view portfolio_redeem = "/portfolio/redeem";
    inline constexpr std::string_view portfolio_split = "/portfolio/split";
    inline constexpr std::string_view portfolio_merge = "/portfolio/merge";
    inline constexpr std::string_view portfolio_withdraw = "/portfolio/withdraw";
    inline constexpr std::string_view maintenance_status = "/maintenance/status";
    inline constexpr std::string_view profiles_me = "/profiles/me";
    inline constexpr std::string_view profiles = "/profiles";
    inline constexpr std::string_view auth_api_tokens_derive = "/auth/api-tokens/derive";

    // Engine.IO handshake path signed for authenticated Socket.IO connections.
    inline constexpr std::string_view socket_io_handshake_path = "/socket.io/?EIO=4&transport=websocket";
}
