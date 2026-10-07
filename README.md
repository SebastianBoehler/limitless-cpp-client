# limitless-cpp-client

Unofficial C++20 client for [Limitless Exchange](https://limitless.exchange) on Base (chain id **8453**).

It follows the shape of [polymarket-cpp-client](https://github.com/SebastianBoehler/polymarket-cpp-client): a CMake library, a REST session, EIP-712 order signing, and a small order book type. Limitless is a single host, uses three `lmts-*` HMAC headers, signs the classic CTF order struct, and streams markets over Socket.IO rather than a raw JSON websocket.

This is not an official Limitless SDK. The supported official clients are TypeScript, Python, Go, and Rust. There is no sandbox. Every call hits production and real USDC.

## Build

Dependencies: CMake 3.22+, a C++20 compiler, libcurl, and OpenSSL. nlohmann/json, IXWebSocket, secp256k1, and keccak are fetched at configure time.

```bash
cmake -S . -B build -DLIMITLESS_CLIENT_BUILD_EXAMPLES=ON -DLIMITLESS_CLIENT_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/limitless_markets_orderbook
./build/limitless_markets_orderbook some-market-slug
```

FetchContent:

```cmake
include(FetchContent)
FetchContent_Declare(
    limitless_client
    GIT_REPOSITORY https://github.com/SebastianBoehler/limitless-cpp-client.git
    GIT_TAG main
)
FetchContent_MakeAvailable(limitless_client)
target_link_libraries(your_target PRIVATE limitless::client)
```

Headers live in `include/limitless/` under namespace `limitless`.

## Public markets and the order book

```cpp
#include "limitless/limitless.hpp"

limitless::ClobClient client(limitless::Environment::base());
limitless::ActiveMarketsQuery query;
query.limit = 5;
query.trade_type = "clob";
auto page = client.get_active_markets(query);
auto book = client.get_orderbook(page.value().markets.front().slug);
auto no_book = limitless::derive_no_book(book.value());
```

`GET /markets/{slug}/orderbook` is the YES book only. Sizes are raw 6-decimal units (1 share = 1,000,000). Prices are decimals from 0 to 1. A NO bid at price `p` is the YES ask at `1 - p`. AMM markets have no book and return HTTP 400. A group slug is not a tradable book; use the child slug.

`venue.exchange` on `GET /markets/{slug}` is the EIP-712 `verifyingContract`. Cache it per market. Do not assume one global exchange: several venue versions are live. Published USDC, conditional-token, and venue addresses are in `limitless::contracts` for reference.

## HMAC request auth

Scoped API tokens are created in the Limitless UI (profile, Api keys) or with `POST /auth/api-tokens/derive` and a Privy identity token. The secret is shown once and is base64. New static `X-API-Key: lmts_...` keys are not issued.

Every signed request sends:

| Header | Value |
| --- | --- |
| `lmts-api-key` | token id |
| `lmts-timestamp` | ISO-8601 UTC, millisecond precision, `Z` suffix, within 30 seconds of the server |
| `lmts-signature` | Base64 HMAC-SHA256 |

Canonical message:

```text
{timestamp}
{METHOD}
{path and query}
{body}
```

GET uses an empty body. The path includes the query string (`/orders/all/btc-100k?onBehalfOf=42`). The secret is base64-decoded before HMAC.

```cpp
client.set_credentials({"token-id", "base64-secret"});
auto profile = client.get_current_profile();
// profile.id is ownerId
// profile.fee_rate_bps is rank.feeRateBps
```

Self-signed orders require the profile trading mode `eoa`. If the account is in `smartWallet` mode the API rejects the signature. Switch with `client.set_trade_wallet_option("eoa")` (`PUT /profiles`).

## EIP-712 orders

Domain from the signing guide:

```text
name: Limitless CTF Exchange
version: 1
chainId: 8453
verifyingContract: venue.exchange
```

The signed `Order` fields are `salt`, `maker`, `signer`, `taker`, `tokenId`, `makerAmount`, `takerAmount`, `expiration`, `nonce`, `feeRateBps`, `side`, `signatureType`. `expiration` and `nonce` are signed as `0`. `side` is `0` buy and `1` sell. `signatureType` `0` is the EOA type named in the signing guide. The API schema also accepts 1, 2, and 3 without naming them here.

`price` is not part of the hash. GTC and FAK still send it on the JSON order: 0.01 through 0.99, at most three decimal places, and `price × shares` must be an exact raw amount. FOK omits `price`, sets `takerAmount` to `1`, and puts the USDC (buy) or shares (sell) in `makerAmount`. Amounts are raw 1e6 units. `makerAmount` must be at least 100 raw units. `maker` and `signer` are sent checksummed (EIP-55).

On a market with `metadata.fee`, `feeRateBps` must match `rank.feeRateBps` from the profile. Sign `0` only when the market is not fee-bearing.

```cpp
limitless::OrderSigner signer(std::getenv("LIMITLESS_PRIVATE_KEY"));
client.set_signer(std::move(signer));

limitless::LimitOrderRequest order;
order.market_slug = market.slug;
order.token_id = *market.tokens->yes;          // or the NO token id
order.verifying_contract = market.venue->exchange;
order.side = limitless::Side::Buy;
order.type = limitless::OrderType::Gtc;
order.price = "0.45";
order.shares = "5";
order.owner_id = profile.id;
order.fee_rate_bps = profile.fee_rate_bps;
auto placed = client.create_order(order);
```

`POST /orders` can return HTTP 425 for a receive window or for maintenance. Do not resubmit the same signed payload after a receive-window 425. Check `GET /maintenance/status` when the body carries a trading-mode code. `POST /v2/orders` is the async place route (`create_order_body(json, true)`).

Approvals, before any order: buy orders need USDC allowed to `venue.exchange`. Sells need conditional tokens allowed to `venue.exchange`, and NegRisk sells also need `venue.adapter`. Collateral is native Base USDC (`limitless::contracts::usdc`, 6 decimals).

## WebSocket

See [docs/websocket.md](docs/websocket.md). Short version: connect to `wss://ws.limitless.exchange` namespace `/markets` with Engine.IO over WebSocket. `MarketsSocket` encodes `subscribe_market_prices` and the authenticated user events. It is a stub, not a full Socket.IO stack.

```cpp
limitless::MarketsSocket socket(limitless::Environment::base());
socket.on_event([](const limitless::SocketIoEvent &event) {
    if (event.name == "orderbookUpdate") {
        // event.args[0] is the documented frame
    }
});
socket.start();
limitless::MarketPriceSubscription subscription;
subscription.market_slugs = {"your-clob-slug"};
socket.subscribe_market_prices(subscription);
```

Pass HMAC credentials before `start()` for `UserStream` (`subscribe_positions`, `subscribe_order_events`, `subscribe_transactions`). Re-sign by calling `start()` again before a late authenticated subscribe, and after the 24-hour disconnect.

## Layout

| Piece | Role |
| --- | --- |
| `Environment` | Base 8453 preset. No testnet preset exists. |
| `HttpClient` / `RestSession` | libcurl keep-alive. Optional proxy and interface bind on REST. |
| `sign_request` | `lmts-*` HMAC |
| `ClobClient` | Markets, books, profiles, orders |
| `OrderSigner` | EIP-712 CTF order |
| `OrderBook` | YES book, NO derivation, versioned socket updates |
| `PositionClient` | Portfolio reads, split/merge/redeem/withdraw |
| `MarketsSocket` / `UserStream` | Socket.IO `/markets` stub |

Wrapped paths are listed in [docs/endpoints.md](docs/endpoints.md). Query enums in the client are the ones published in OpenAPI (for example historical `interval`: `5m`, `1h`, `6h`, `1d`, `1w`, `1m`, `all`).

## Limits and other notes

- Chain: Base mainnet `8453` only.
- REST: `https://api.limitless.exchange`
- OpenAPI: `https://api.limitless.exchange/api-json`
- Docs describe HTTP 429 and an optional `Retry-After`. They do not publish a numeric quota.
- The docs note georestrictions (including the US) in the SDK disclaimers.
- Orderbook `minSize` / `maxSpread` may be strings on REST and numbers on the socket. This client accepts both.
- `sortBy` on active markets is an empty object in OpenAPI, so it is not sent.

## License

MIT. See [LICENSE](LICENSE).
