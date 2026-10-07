# Socket.IO market stream

Limitless market data is a [Socket.IO](https://socket.io/docs/v4/socket-io-protocol/) namespace, not the raw JSON websocket Polymarket uses.

| | Polymarket CLOB | Limitless |
| --- | --- | --- |
| URL | `wss://ws-subscriptions-clob.polymarket.com/ws/market` | `wss://ws.limitless.exchange` |
| Framing | One JSON object per websocket message | Engine.IO v4 packets inside websocket frames, Socket.IO namespace `/markets` |
| Subscribe | `{"assets_ids":[...],"type":"market"}` | event `subscribe_market_prices` with `{ "marketSlugs": [...] }` |
| Book | `event_type: "book"` / `price_change` diffs | `orderbookUpdate` full book; replace local state |
| Auth | API creds inside the user-channel subscribe | HMAC headers on the HTTP upgrade |

## Handshake

1. Open `wss://ws.limitless.exchange/socket.io/?EIO=4&transport=websocket` with websocket transport only. There is no long-polling fallback.
2. The server sends an Engine.IO open packet: `0{"sid":"...","pingInterval":25000,"pingTimeout":20000}`.
3. The client joins the namespace with `40/markets,`.
4. The server answers `40/markets,{"sid":"..."}`.
5. Emit `42/markets,["subscribe_market_prices",{"marketSlugs":["<slug>"]}]`.

`WebSocketClient` performs that upgrade itself with OpenSSL (RFC 6455 frames, masked client payloads). Many distro libcurl builds, including Ubuntu 24.04's 8.5 package, are compiled without the websocket protocol, so the client does not call `curl_ws_*`. REST still uses libcurl. A configured HTTP proxy is refused for the socket rather than falling back to a direct connection; proxy support on REST is unchanged.

Engine.IO ping is a text frame `2`. Reply with `3`. Do not send websocket PING frames. The load balancer closes the socket after 24 hours with code 1006; open a new connection and emit the subscription again.

`subscribe_market_prices` replaces the previous set on that connection. The server then sends one full `orderbookUpdate` per CLOB slug. AMM subscriptions use `marketAddresses` and arrive as `newPriceData`. Send both fields in one emit when you want both.

## Authenticated channels

Sign this exact canonical string with the base64-decoded token secret and put the headers on the upgrade request:

```text
{ISO-8601 timestamp}\nGET\n/socket.io/?EIO=4&transport=websocket\n
```

Headers: `lmts-api-key`, `lmts-timestamp`, `lmts-signature`.

`subscribe_positions`, `subscribe_order_events`, and `subscribe_transactions` are checked against that handshake timestamp and must be emitted within 30 seconds. A later authenticated subscribe needs a new connection. `WebSocketClient::reconnect()` signs the handshake again and replays the last subscription set.

Event names and payloads are documented at <https://docs.limitless.exchange/developers/websocket/overview>.
