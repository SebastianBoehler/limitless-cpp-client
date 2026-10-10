# Polymarket learnings applied to Limitless

I especially appreciate Bill's efficiency and performance improvements in
Polymarket #33. This port keeps Limitless's own HMAC and Socket.IO contracts.

## Changes

- Parsed book prices must be finite and in [0, 1]. Raw sizes must be non-negative
  integer values. Floating sizes must fit the signed integer conversion and have
  no fractional part. Duplicate side prices are rejected after sorting.
- A rejected versioned update leaves the previous book and version unchanged.
- Complemented books preserve sorted order when the source came from the parser.
  An order check avoids sorting those books again. Manually constructed unsorted
  books still produce sorted complements.
- Socket.IO event dispatch references the already decoded event instead of copying
  its JSON arguments a second time. The public codec's packet data is preserved.
- Replacing a running movable socket stops and joins the old implementation before
  destroying its callback state. A loopback Socket.IO fixture covers event delivery,
  move construction, and replacement of an active implementation.
- Order amount accumulation checks the maximum integer before multiplication.
  Existing product-overflow and exact-unit checks remain in place.

Existing persistent HTTP reuse, compression, invariant type hashes, fixed-word
integer encoding, and exact decimal amount math already implement the relevant
Polymarket ideas. The documented single-threaded REST session is unchanged.

## Measurements

Apple Silicon, AppleClang 21, Release build, 10 October 2026. Before is one baseline
run at `bca936f`; after is the median of three final runs. Each side has 100 levels.
These are local CPU timings; they exclude JSON text decoding and network latency.

| Operation | Before | After |
| --- | ---: | ---: |
| Derive complementary book | 1.33 us | 1.02 us |
| Parse an existing JSON book | 8.21 us | 8.31 us |

The complement improvement is about 23% in this measurement. No parser speedup is
claimed: stronger validation keeps its measured cost approximately unchanged.

```sh
cmake --build build --target limitless_hot_paths --parallel 2
./build/limitless_hot_paths
ctest --test-dir build --output-on-failure
```

## Validation

All seven CTest targets pass in Release and Debug with AddressSanitizer and
UndefinedBehaviorSanitizer. The sanitizer build targets macOS 12. The new book
regressions fail on the original code. They cover negative/nonfinite prices,
negative/fractional/out-of-range sizes, duplicate levels, atomic version rejection,
and unsorted manually constructed complements. Overflow amount rejection remains
covered. The local Socket.IO lifecycle fixture passes under sanitizers.
No live order or account mutation is sent.

## History review

Reviewed Polymarket history through `6a4ee0f` (PR #33), including earlier transport,
signing, book parsing, lifecycle, packaging, and CI changes. Sources:

| Source change | Lesson | Application here |
| --- | --- | --- |
| [#33 / 6a4ee0f](https://github.com/SebastianBoehler/polymarket-cpp-client/commit/6a4ee0f) | Reuse HTTP handles, enable compression, isolate orders, avoid redundant metadata reads. | Both clients already reuse handles and enable compression. Opinion now isolates orders. Limitless's session is explicitly single-threaded. |
| [#29 / 7c4ae7e](https://github.com/SebastianBoehler/polymarket-cpp-client/commit/7c4ae7e) | Avoid repeated JSON parsing, numeric allocations, and invariant hashing; enforce configured routes. | Opinion ports integer encoding, cached hashes, single REST parsing, and route rejection. Limitless removes the extra event dispatch copy. |
| [a3cd1c8](https://github.com/SebastianBoehler/polymarket-cpp-client/commit/a3cd1c8) | Validate before publishing state; reject invalid numbers and duplicates; stop workers before destroying their state. | Both books validate before mutation. Limitless also stops a replaced socket implementation before its callback state is destroyed. |
| [1945e73](https://github.com/SebastianBoehler/polymarket-cpp-client/commit/1945e73), [33db662](https://github.com/SebastianBoehler/polymarket-cpp-client/commit/33db662) | Keep signing arithmetic exact and obey venue-specific tick rules. | Both clients already use decimal strings for amounts. Opinion's price comparison stays exact. Limitless now checks integer accumulation before overflow. Polymarket tick rules are not copied. |
| [5d348c9](https://github.com/SebastianBoehler/polymarket-cpp-client/commit/5d348c9), [ccc5a0b](https://github.com/SebastianBoehler/polymarket-cpp-client/commit/ccc5a0b) | Auth identities and order types must match the venue's protocol. | Signing vectors remain unchanged. Limitless uses HMAC; Opinion uses its own API-key auth. No Polymarket auth or FAK serialization is transplanted. |
| [#31 / 8e674d8](https://github.com/SebastianBoehler/polymarket-cpp-client/commit/8e674d8) | Never automatically retry order mutations. | No automatic retries are added. Existing HTTP/API error metadata remains available. |
| [#32 / 151c440](https://github.com/SebastianBoehler/polymarket-cpp-client/commit/151c440), [#34](https://github.com/SebastianBoehler/polymarket-cpp-client/pull/34) | Settlement success needs final chain evidence, not an intermediate stream event. | These clients do not expose the same settlement helper or status contract. No fabricated counterpart is introduced. |
| [880e082](https://github.com/SebastianBoehler/polymarket-cpp-client/commit/880e082), [95969e8](https://github.com/SebastianBoehler/polymarket-cpp-client/commit/95969e8), [94a809a](https://github.com/SebastianBoehler/polymarket-cpp-client/commit/94a809a) | Namespace public headers, validate dependency graphs, and bound compiler parallelism. | Headers are already namespaced; existing CI already limits builds to two workers. Validation here covers source builds and public examples, not a relocatable installed SDK. |

Polymarket's Gamma pools, tick/negative-risk caches, approvals, oracle/indexer helpers,
preproduction deployment, and heartbeat statistics have no direct equivalent here.
Polymarket's macOS floating `from_chars` regression is avoided: these ports do not
use floating `from_chars` or convert exact order integers through `double`.

