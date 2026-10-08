<div align="center">

# limitless-cpp-client

**A C++20 client for Limitless Exchange.**<br>
CLOB REST and Socket.IO streaming, HMAC scoped-token auth, and EIP-712 order signing on Base.

[![build](https://github.com/SebastianBoehler/limitless-cpp-client/actions/workflows/build.yml/badge.svg)](https://github.com/SebastianBoehler/limitless-cpp-client/actions/workflows/build.yml)
[![release](https://img.shields.io/github/v/release/SebastianBoehler/limitless-cpp-client)](https://github.com/SebastianBoehler/limitless-cpp-client/releases)
[![license](https://img.shields.io/github/license/SebastianBoehler/limitless-cpp-client)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.22%2B-064F8C?logo=cmake&logoColor=white)](https://cmake.org)
[![platforms](https://img.shields.io/badge/platforms-Linux%20%7C%20macOS-lightgrey)](#requirements)
[![stars](https://img.shields.io/github/stars/SebastianBoehler/limitless-cpp-client?style=flat&logo=github)](https://github.com/SebastianBoehler/limitless-cpp-client/stargazers)
[![PRs welcome](https://img.shields.io/badge/PRs-welcome-brightgreen)](CONTRIBUTING.md)

[Quick start](#quick-start) ·
[Features](#features) ·
[Installation](#installation) ·
[Examples](#examples) ·
[Docs](#documentation) ·
[Contributing](#contributing)

</div>

---

Unofficial client for [Limitless Exchange](https://limitless.exchange), the prediction market on Base. It follows the shape of [polymarket-cpp-client](https://github.com/SebastianBoehler/polymarket-cpp-client): a CMake library, a REST session, EIP-712 order signing, and a small order book type. Limitless is a single host, uses three `lmts-*` HMAC headers, signs the classic CTF order struct, and streams markets over Socket.IO rather than a raw JSON websocket.

This is not an official Limitless SDK. The supported official clients are TypeScript, Python, Go, and Rust. There is no sandbox. Every live call hits production and real USDC. The API reference is [docs.limitless.exchange](https://docs.limitless.exchange).

## C++ prediction-market clients

Part of a collection of C++20 clients for prediction markets:

- [Polymarket](https://github.com/SebastianBoehler/polymarket-cpp-client)
- [Limitless Exchange](https://github.com/SebastianBoehler/limitless-cpp-client)
- [Opinion.trade](https://github.com/SebastianBoehler/opinion-cpp-client)

Explore the other clients for market data, order signing, and trading on each platform.

## Quick start

```cpp
#include "limitless/limitless.hpp"

#include <cstdlib>
#include <iostream>

int main()
{
    const char *token_id = std::getenv("LIMITLESS_TOKEN_ID");
    const char *token_secret = std::getenv("LIMITLESS_TOKEN_SECRET");
    const char *private_key = std::getenv("LIMITLESS_PRIVATE_KEY");
    if (!token_id || !token_secret || !private_key) return 1;

    limitless::ClobClient client(limitless::Environment::base());
    client.set_credentials({token_id, token_secret});
    client.set_signer(limitless::OrderSigner(private_key));

    const auto profile = client.get_current_profile();
    if (!profile) {
        std::cerr << profile.error().message << "\n";
        return 1;
    }

    limitless::LimitOrderRequest order;
    order.market_slug = "<clob slug>";
    order.token_id = "<yes or no token id>";
    order.verifying_contract = "<venue.exchange>";
    order.side = limitless::Side::Buy;
    order.type = limitless::OrderType::Gtc;
    order.price = "0.45";
    order.shares = "5";
    order.owner_id = profile.value().id;
    order.fee_rate_bps = profile.value().fee_rate_bps;

    const auto placed = client.create_order(order);  // POST /orders
    if (!placed) {
        std::cerr << placed.error().message << "\n";
        return 1;
    }
    std::cout << placed.value().dump() << "\n";
}
```

`profile.id` is `ownerId`. `profile.fee_rate_bps` is `rank.feeRateBps`. Self-signed orders need the profile in `eoa` mode. See [Trading](#trading) and [Examples](#examples).

## Features

| Area | What you get |
| --- | --- |
| **Trading** | EIP-712 CTF orders on Base (chain id 8453). GTC, FAK, and FOK. `POST /orders`, async `POST /v2/orders`, cancel, cancel-replace, and heartbeat |
| **Market data** | REST active markets, search, books, prices, events, and maintenance. Socket.IO namespace `/markets` |
| **Auth** | Scoped-token HMAC-SHA256. Headers `lmts-api-key`, `lmts-timestamp`, and `lmts-signature` |
| **Portfolio** | Positions, trades, history, allowance, split, merge, redeem, and withdraw |
| **Networking** | REST proxy URL and interface binding through `NetworkOptions`. The Socket.IO client does not apply that route |
| **Errors** | `Result<T>` and `SdkError` (transport, API, auth, rate limit, parse, signing) |

`MarketsSocket` joins `/markets`, answers Engine.IO pings, and emits the documented subscribe events. It is a stub, not a full Socket.IO stack. Notes: [docs/websocket.md](docs/websocket.md).

## Requirements

- CMake 3.22+ and a C++20 compiler
- libcurl and OpenSSL
- Linux or macOS

nlohmann/json, IXWebSocket, secp256k1, and keccak are fetched and pinned by hash at configure time.

## Installation

### CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    limitless_client
    GIT_REPOSITORY https://github.com/SebastianBoehler/limitless-cpp-client.git
    GIT_TAG main # pin a commit or release tag in production
)
FetchContent_MakeAvailable(limitless_client)

target_link_libraries(your_target PRIVATE limitless::client)
```

Headers live in `include/limitless/` under namespace `limitless`. The umbrella header is `limitless/limitless.hpp`.

### From source

```bash
./build.sh            # configure, build the library and example, run tests
# or step by step:
cmake -S . -B build -DLIMITLESS_CLIENT_BUILD_EXAMPLES=ON -DLIMITLESS_CLIENT_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix <install_prefix>
```

## Usage

### Public markets and the order book

`GET /markets/{slug}/orderbook` is the YES book only. Sizes are raw 6-decimal units (1 share = 1,000,000). Prices are decimals from 0 to 1. A NO bid at price `p` is the YES ask at `1 - p`. AMM markets have no book and return HTTP 400. A group slug is not a tradable book; use the child slug.

```cpp
limitless::ClobClient client(limitless::Environment::base());
limitless::ActiveMarketsQuery query;
query.limit = 5;
query.trade_type = "clob";
auto page = client.get_active_markets(query);
if (!page || page.value().markets.empty()) return;

auto book = client.get_orderbook(page.value().markets.front().slug);
if (book) {
    auto no_book = limitless::derive_no_book(book.value());
}
```

`venue.exchange` on `GET /markets/{slug}` is the EIP-712 `verifyingContract`. Cache it per market. Do not assume one global exchange: several venue versions are live. Published USDC, conditional-token, and venue addresses are in `limitless::contracts` for reference.

### HMAC request auth

Scoped API tokens are created in the Limitless UI (profile, Api keys) or with `POST /auth/api-tokens/derive` and a Privy identity token (`ClobClient::derive_api_token`). The secret is shown once and is base64. New static `X-API-Key: lmts_...` keys are not issued.

Every signed request sends `lmts-api-key`, `lmts-timestamp` (ISO-8601 UTC, millisecond precision, `Z` suffix, within 30 seconds of the server), and `lmts-signature` (Base64 HMAC-SHA256).

```text
{timestamp}
{METHOD}
{path and query}
{body}
```

GET uses an empty body. The path includes the query string (`/orders/all/btc-100k?onBehalfOf=42`). The secret is base64-decoded before HMAC.

Self-signed orders require the profile trading mode `eoa`. If the account is in `smartWallet` mode the API rejects the signature. Switch with `client.set_trade_wallet_option("eoa")` (`PUT /profiles`).

### Trading

Domain from the [signing guide](https://docs.limitless.exchange/developers/eip712-signing):

```text
name: Limitless CTF Exchange
version: 1
chainId: 8453
verifyingContract: venue.exchange
```

The signed `Order` fields are `salt`, `maker`, `signer`, `taker`, `tokenId`, `makerAmount`, `takerAmount`, `expiration`, `nonce`, `feeRateBps`, `side`, `signatureType`. `expiration` and `nonce` are signed as `0`. `side` is `0` buy and `1` sell. `signatureType` `0` is the EOA type named in the signing guide. The API schema also accepts 1, 2, and 3.

`price` is not part of the hash. GTC and FAK still send it on the JSON order: 0.01 through 0.99, at most three decimal places, and `price × shares` must be an exact raw amount. FOK omits `price`, sets `takerAmount` to `1`, and puts the USDC (buy) or shares (sell) in `makerAmount`. Amounts are raw 1e6 units. `makerAmount` must be at least 100 raw units. `maker` and `signer` are sent checksummed (EIP-55).

On a market with `metadata.fee`, `feeRateBps` must match `rank.feeRateBps` from the profile. Sign `0` only when the market is not fee-bearing.

`POST /orders` can return HTTP 425 for a receive window or for maintenance. Do not resubmit the same signed payload after a receive-window 425. Check `GET /maintenance/status` when the body carries a trading-mode code. `POST /v2/orders` is the async place route (`create_order_body(json, true)`).

Approvals, before any order: buy orders need USDC allowed to `venue.exchange`. Sells need conditional tokens allowed to `venue.exchange`, and NegRisk sells also need `venue.adapter`. Collateral is native Base USDC (`limitless::contracts::usdc`, 6 decimals).

Wrapped paths are listed in [docs/endpoints.md](docs/endpoints.md). Query enums in the client are the ones published in OpenAPI (for example historical `interval`: `5m`, `1h`, `6h`, `1d`, `1w`, `1m`, `all`).

### Socket.IO

Connect to `wss://ws.limitless.exchange`, namespace `/markets`, Engine.IO over WebSocket. Public market data needs no HMAC. Authenticated channels need the handshake signed before `start()`.

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

`UserStream` covers `subscribe_positions`, `subscribe_order_events`, and `subscribe_transactions`. Those must be emitted within 30 seconds of the handshake timestamp. Call `start()` again before a late authenticated subscribe, and after the 24-hour disconnect. Event payloads: [WebSocket overview](https://docs.limitless.exchange/developers/websocket/overview).

### Environment

`Environment::base()` is the only preset. Limitless does not publish a testnet.

| | Value |
| --- | --- |
| Chain | Base mainnet, chain id 8453 |
| REST | `https://api.limitless.exchange` |
| OpenAPI | `https://api.limitless.exchange/api-json` |
| WebSocket | `wss://ws.limitless.exchange/markets` |
| Collateral | USDC `0x833589fCD6eDb6E08f4c7C32D4f71b54bdA02913` (6 decimals) |
| Conditional tokens | `0xC9c98965297Bc527861c898329Ee280632B76e18` |

Docs describe HTTP 429 and an optional `Retry-After`. They do not publish a numeric quota. Orderbook `minSize` / `maxSpread` may be strings on REST and numbers on the socket. This client accepts both. `sortBy` on active markets is an empty object in OpenAPI, so it is not sent.

## Examples

Build with `-DLIMITLESS_CLIENT_BUILD_EXAMPLES=ON`. The binary is `build/limitless_markets_orderbook`.

| Example | What it does |
| --- | --- |
| `limitless_markets_orderbook` | Lists active CLOB markets and prints one YES book plus the derived NO book. Pass a slug, or `--limit N` |

```bash
./build/limitless_markets_orderbook
./build/limitless_markets_orderbook some-market-slug
./build/limitless_markets_orderbook --limit 8
```

The example calls the live production API.

## Documentation

| Guide | Topic |
| --- | --- |
| [Documentation](https://docs.limitless.exchange) | Limitless Exchange docs home |
| [For developers](https://docs.limitless.exchange/developers/introduction) | REST and WebSocket overview |
| [Migrate from Polymarket](https://docs.limitless.exchange/developers/migrate-from-polymarket) | Concept and endpoint map |
| [Authentication](https://docs.limitless.exchange/developers/authentication) | Scoped tokens and HMAC |
| [EIP-712 signing](https://docs.limitless.exchange/developers/eip712-signing) | Order typed data |
| [Venue system](https://docs.limitless.exchange/developers/venue-system) | Per-market exchange and adapter |
| [WebSocket overview](https://docs.limitless.exchange/developers/websocket/overview) | Handshake, events, authenticated channels |
| [API reference](https://docs.limitless.exchange/api-reference/introduction) | REST reference |
| [Wrapped endpoints](docs/endpoints.md) | Paths this client calls |
| [Socket.IO notes](docs/websocket.md) | Framing `MarketsSocket` implements |

Official SDKs: [TypeScript](https://github.com/limitless-labs-group/limitless-exchange-ts-sdk), [Python](https://github.com/limitless-labs-group/limitless-sdk), [Go](https://github.com/limitless-labs-group/limitless-exchange-go-sdk), [Rust](https://github.com/limitless-labs-group/limitless-exchange-rust-sdk). Builders chat: [Telegram](https://t.me/LimitlessBuildersChat).

## Contributing

Contributions are welcome. [CONTRIBUTING.md](CONTRIBUTING.md) lists the local build and the checks CI runs.

```bash
./build.sh
```

Bugs and feature requests go to [Issues](https://github.com/SebastianBoehler/limitless-cpp-client/issues).
Security reports follow [SECURITY.md](SECURITY.md).

## Disclaimer

This is an independent open-source project, not affiliated with or endorsed by Limitless. Trading involves risk of loss. The Limitless docs note geographic restrictions, including the United States, in the SDK disclaimers. A proxy does not change whether you are allowed to trade. You are responsible for the venue terms and the laws where you are.

## License

[MIT](LICENSE)
