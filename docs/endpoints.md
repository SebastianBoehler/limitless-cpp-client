# REST paths wrapped by this client

Base URL: `https://api.limitless.exchange`

Paths below are taken from the OpenAPI document at `https://api.limitless.exchange/api-json` unless noted. This client does not add routes that are absent from both that document and the current docs.

## Public

| Method | Path |
| --- | --- |
| GET | `/markets/active` |
| GET | `/markets/active/{categoryId}` |
| GET | `/markets/active/slugs` |
| GET | `/markets/categories/count` |
| GET | `/markets/search` |
| GET | `/markets/{addressOrSlug}` |
| GET | `/markets/stable/{slug}` |
| GET | `/markets/{slug}/orderbook` |
| GET | `/markets/{slug}/historical-price` |
| GET | `/markets/{slug}/events` |
| GET | `/markets/{slug}/get-feed-events` |
| GET | `/markets/timeline` |
| GET | `/markets/{slug}/timeline` |
| GET | `/markets/{addressOrSlug}/oracle-candles` |
| GET | `/maintenance/status` |
| GET | `/portfolio/{account}/positions` |
| GET | `/portfolio/{account}/traded-volume` |
| GET | `/portfolio/{account}/history` |
| GET | `/portfolio/{account}/realized-pnl` |

`GET /maintenance/status` is documented and returned HTTP 200 on 2026-10-07. It was not in the OpenAPI path list fetched the same day.

## HMAC (`lmts-api-key`, `lmts-timestamp`, `lmts-signature`)

| Method | Path |
| --- | --- |
| GET | `/profiles/me` |
| GET | `/profiles/{account}` |
| PUT | `/profiles` |
| GET | `/markets/{slug}/user-orders` |
| POST | `/orders` |
| POST | `/v2/orders` |
| DELETE | `/orders/{orderId}` |
| POST | `/orders/cancel` |
| POST | `/orders/batch-cancel` |
| DELETE | `/orders/all/{slug}` |
| POST | `/orders/cancel-replace` |
| POST | `/orders/cancel-replace/batch` |
| POST | `/orders/status/batch` |
| GET | `/v2/orders/status/{id}` |
| POST | `/heartbeats` |
| GET | `/portfolio/positions` |
| GET | `/portfolio/trades` |
| GET | `/portfolio/history` |
| GET | `/portfolio/pnl-chart` |
| GET | `/portfolio/points` |
| GET | `/portfolio/trading/allowance` |
| POST | `/portfolio/redeem` |
| POST | `/portfolio/split` |
| POST | `/portfolio/merge` |
| POST | `/portfolio/withdraw` |

`GET /profiles/me`, `GET /profiles/{account}`, and `PUT /profiles` are in the authentication and profile docs. Live calls without credentials returned 401, not 404.

`POST /auth/api-tokens/derive` uses a Privy identity token in the `identity: Bearer ...` header, not HMAC. The docs describe it, and a live unauthenticated POST returned 401.

## Present in OpenAPI, not wrapped in v0.1

Navigation, referrals, packs, user-generated markets, LMTS token stats, profile search, partner allowance retry, AMM server-wallet buy/sell, and `POST /orders/cancel-batch` (the combined `POST /orders/batch-cancel` is wrapped). Partner account create/list routes are described in the docs index; this tree does not guess their request bodies.
