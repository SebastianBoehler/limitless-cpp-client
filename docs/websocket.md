# Socket.IO market stream

Limitless does not speak the raw JSON websocket protocol used by Polymarket. The market stream is Socket.IO on Engine.IO v4, websocket transport only.

| Item | Value |
| --- | --- |
| URL | `wss://ws.limitless.exchange` |
| Namespace | `/markets` |
| Engine.IO path | `/socket.io/?EIO=4&transport=websocket` |
| Full websocket URL | `wss://ws.limitless.exchange/socket.io/?EIO=4&transport=websocket` |

`MarketsSocket` is a stub for that one namespace. It joins `/markets`, answers Engine.IO ping packets with pong, and emits the documented subscribe events. It does not implement polling, binary attachments, or acknowledgements.

## Handshake authentication

Public market data needs no headers. Positions, order events, and transaction events need an HMAC handshake. The canonical message is fixed:

```text
{ISO-8601 timestamp}
GET
/socket.io/?EIO=4&transport=websocket

```

The body component is empty. Sign it the same way as REST (`lmts-api-key`, `lmts-timestamp`, `lmts-signature`), with the secret base64-decoded before HMAC-SHA256. `sign_websocket_handshake` builds those headers.

The server re-checks that timestamp on `subscribe_positions`, `subscribe_order_events`, and `subscribe_transactions`. It must be within 30 seconds. This client refuses those emits when the local handshake is older than 25 seconds. Call `start()` again to open a new connection with a fresh timestamp. Socket.IO would otherwise replay the original headers on reconnect, and those headers are already stale.

There is no auth-confirmation event. A successful authenticated subscribe is a `system` message. A failure arrives on `exception`.

## Framing

Engine.IO packet types used here:

| Prefix | Meaning | Client action |
| --- | --- | --- |
| `0{...}` | open | send namespace connect |
| `2` | ping | send `3` (pong) |
| `3` | pong | ignore |
| `40/markets,` | connect to `/markets` | sent by the client after open |
| `42/markets,[... ]` | event | decode name and JSON args |

Do not send WebSocket protocol PING frames yourself. The server heartbeat is the Engine.IO ping above. Connections are closed by the load balancer after 24 hours with code 1006 and no close frame. Subscriptions are not kept across connections. `subscribe_market_prices` and the authenticated subscribes replace the previous set rather than adding to it.

`SocketIoCodec` encodes and decodes these frames without a network connection.

## Events this stub emits

Documented client events from the websocket overview:

| Event | Auth | Payload this client sends |
| --- | --- | --- |
| `subscribe_market_prices` | no | `{ marketAddresses?, marketSlugs? }` |
| `subscribe_positions` | HMAC | same object |
| `subscribe_order_events` | HMAC | no payload |
| `subscribe_transactions` | HMAC | no payload |

`subscribe_transactions` is named in the overview and delivers `tx`. The overview does not publish a payload, so the stub emits the event with no arguments.

These overview events are not wrapped with a typed helper: `subscribe_market_lifecycle`, `unsubscribe_market_lifecycle`, `subscribe_oracle_price_data`, `unsubscribe_oracle_price_data`, `subscribe_unrealized_pnl`, `unsubscribe_unrealized_pnl`. The overview also names `subscribe_live_sports`, `subscribe_live_esports`, and `subscribe_ugm_live_offers` and says they are undocumented.

## Server events to handle

| Event | Use |
| --- | --- |
| `orderbookUpdate` | Full YES book for a CLOB slug. Replace local state. `OrderBook::apply_update` drops an older `version` and ignores version `0` after a live frame. |
| `newPriceData` | AMM prices |
| `positions` | Balance update after an authenticated `subscribe_positions` |
| `orderEvent` | OME and settlement frames. Read `source`, then `type`. |
| `tx` | Settlement result, `CONFIRMED` or `FAILED` |
| `marketCreated`, `marketResolved` | Lifecycle |
| `oraclePriceData` | Oracle feed |
| `unrealizedPnlProjectionChanged` | Hint to refetch the REST leaderboard |
| `system`, `exception`, `error` | Ack and errors |

`orderbookUpdate.orderbook` matches `GET /markets/{slug}/orderbook` except REST omits nothing the socket snapshot lacks `lastTradePrice` for, and REST sends `minSize` / `maxSpread` as strings while the socket may send numbers. The book is YES-side only. `derive_no_book` flips bids and asks and replaces price `p` with `1 - p`.

Initial subscribe pushes one full book per CLOB slug. A re-subscribe replaces the previous subscription, so keep every slug you still want in the next call.
