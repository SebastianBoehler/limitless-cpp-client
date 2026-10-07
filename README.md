# limitless-cpp-client

C++20 SDK for [Limitless Exchange](https://limitless.exchange) on Base. REST market data and trading, HMAC scoped-token auth, EIP-712 order signing, and a Socket.IO client for `wss://ws.limitless.exchange/markets`.

There is no official C++ SDK. Limitless publishes TypeScript, Python, Go, and Rust clients; this library is an independent client in the same shape as [polymarket-cpp-client](https://github.com/SebastianBoehler/polymarket-cpp-client).

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.22%2B-064F8C?logo=cmake&logoColor=white)](https://cmake.org)
[![license](https://img.shields.io/badge/license-MIT-green)](LICENSE)

## Install

CMake 3.22+, a C++20 compiler, libcurl, OpenSSL, and zlib. nlohmann/json is fetched at configure time.

```bash
./build.sh
# or
cmake -S . -B build -DLIMITLESS_CLIENT_BUILD_EXAMPLES=ON -DLIMITLESS_CLIENT_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/public_markets
```

FetchContent:

```cmake
include(FetchContent)
FetchContent_Declare(
    limitless_client
    GIT_REPOSITORY https://github.com/SebastianBoehler/limitless-cpp-client.git
    GIT_TAG v0.1.0
)
FetchContent_MakeAvailable(limitless_client)
target_link_libraries(your_target PRIVATE limitless::client)
```

## Public markets

```cpp
#include "limitless/clob_client.hpp"

limitless::ClobClient client(limitless::Environment::production());
limitless::ActiveMarketsQuery query;
query.limit = 5;
query.trade_type = "clob";
auto markets = client.get_active_markets(query);
auto book = client.get_orderbook(markets.value().data.front().slug);
```

`GET /markets/active` and `GET /markets/{slug}/orderbook` are public. The book is the YES side in raw 6-decimal sizes. `derive_no_book()` mirrors it with `1 - price`.

## Trading

Scoped API tokens sign every private request with three headers: `lmts-api-key`, `lmts-timestamp`, `lmts-signature`. The canonical string is `{timestamp}\n{METHOD}\n{path+query}\n{body}`. The secret is base64-decoded before HMAC-SHA256. Create the token in the Limitless profile UI, or with `POST /auth/api-tokens/derive` when you already have a Privy identity token. Legacy static `X-API-Key` values are not issued to new users and this client does not send them.

Orders are a second signature. Domain `Limitless CTF Exchange` / `1` / chain id **8453** / `verifyingContract = venue.exchange` from `GET /markets/{slug}`. `expiration` and `nonce` are signed as `0`. On markets with `metadata.fee`, `feeRateBps` must be `rank.feeRateBps` from `GET /profiles/me`. Self-signed orders need the profile in EOA mode (`PUT /profiles` with `{"tradeWalletOption":"eoa"}`).

```cpp
client.set_credentials({std::getenv("LMTS_TOKEN_ID"), std::getenv("LMTS_TOKEN_SECRET")});
client.set_signer(std::getenv("PRIVATE_KEY"));

limitless::OrderRequest order;
order.market_slug = "<clob slug>";
order.side = limitless::OrderSide::Buy;
order.type = limitless::OrderType::Gtc;
order.price = "0.50";
order.size = "10";
auto response = client.create_and_post_order(order);  // POST /orders
```

`ownerId` is the profile id. Leave it unset and the client reads `GET /profiles/me`. `async_submit` posts to `POST /v2/orders`. Cancels cover `DELETE /orders/{orderId}`, `POST /orders/cancel`, `POST /orders/batch-cancel`, and `DELETE /orders/all/{slug}`.

## WebSocket

Polymarket streams raw JSON. Limitless uses Socket.IO on namespace `/markets` (Engine.IO v4, websocket transport only). `WebSocketClient` opens a direct TLS websocket and speaks that framing: namespace connect, server ping/pong, and `subscribe_market_prices`. Authenticated events need HMAC headers on the upgrade and a fresh connection if you subscribe more than 30 seconds later. Protocol notes: [docs/websocket.md](docs/websocket.md).

```cpp
limitless::WebSocketClient ws;
ws.subscribe_market_prices({"<slug>"});
ws.connect();
ws.poll(5000);
```

## Environment

`Environment::production()` is the only preset. Limitless does not publish a testnet.

| | Value |
| --- | --- |
| Chain | Base mainnet, chain id 8453 |
| REST | `https://api.limitless.exchange` |
| OpenAPI | `https://api.limitless.exchange/api-json` |
| WebSocket | `wss://ws.limitless.exchange/markets` |
| Collateral | USDC `0x833589fCD6eDb6E08f4c7C32D4f71b54bdA02913` (6 decimals) |
| Conditional tokens | `0xC9c98965297Bc527861c898329Ee280632B76e18` |

Venue exchange and adapter addresses come from each market. The contract list in `environment.hpp` matches the [smart contracts](https://docs.limitless.exchange/user-guide/smart-contracts) page and is not a substitute for `venue.exchange`.

## What v1 calls

Public: `GET /markets/active`, `GET /markets/{slug}`, `GET /markets/search`, `GET /markets/{slug}/orderbook`, `GET /markets/{slug}/historical-price`, `GET /markets/{slug}/events`, `GET /markets/active/slugs`, `GET /markets/categories/count`, `GET /maintenance/status`.

Private: `GET /profiles/me`, `PUT /profiles`, `GET /portfolio/positions`, `GET /portfolio/trades`, `GET /portfolio/history`, `POST /portfolio/split`, `POST /portfolio/merge`, `POST /portfolio/redeem`, `GET /markets/{slug}/user-orders`, `POST /orders`, `POST /v2/orders`, `DELETE /orders/{orderId}`, `POST /orders/cancel`, `POST /orders/batch-cancel`, `DELETE /orders/all/{slug}`, `POST /orders/status/batch`, `POST /heartbeats`.

`ClobClient::request` signs any other path you pass. Paths above are the ones documented in the OpenAPI spec and the developer docs linked below.

## Docs

- [Documentation](https://docs.limitless.exchange)
- [For developers](https://docs.limitless.exchange/developers/introduction)
- [Migrate from Polymarket](https://docs.limitless.exchange/developers/migrate-from-polymarket)
- [Authentication](https://docs.limitless.exchange/developers/authentication)
- [EIP-712 signing](https://docs.limitless.exchange/developers/eip712-signing)
- [Venue system](https://docs.limitless.exchange/developers/venue-system)
- [WebSocket overview](https://docs.limitless.exchange/developers/websocket/overview)
- [API reference](https://docs.limitless.exchange/api-reference/introduction)

Official SDKs: [TypeScript](https://github.com/limitless-labs-group/limitless-exchange-ts-sdk), [Python](https://github.com/limitless-labs-group/limitless-sdk), [Go](https://github.com/limitless-labs-group/limitless-exchange-go-sdk), [Rust](https://github.com/limitless-labs-group/limitless-exchange-rust-sdk). Builders chat: [Telegram](https://t.me/LimitlessBuildersChat).

## Disclaimer

Independent open-source project, not affiliated with or endorsed by Limitless. Trading can lose money. Limitless documents geographic restrictions; a proxy does not change whether you are allowed to trade. You are responsible for the venue terms and the laws where you are.

## License

[MIT](LICENSE)
