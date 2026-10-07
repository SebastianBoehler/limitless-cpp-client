# Contributing

Contributions are welcome. This repository is a C++20 client for the published Limitless Exchange REST and Socket.IO APIs. Do not add a route, field, or signing rule that is not in [the docs](https://docs.limitless.exchange) or the OpenAPI document at `https://api.limitless.exchange/api-json`.

## Build

Requirements: CMake 3.22+, a C++20 compiler, libcurl, and OpenSSL.

```bash
./build.sh
```

That configures the library, the `limitless_markets_orderbook` example, and the unit tests, then runs `ctest`. [GitHub Actions](.github/workflows/build.yml) runs the same CMake build on Ubuntu 22.04 and macOS 15, in Debug and Release.

The example calls production. The tests do not.

## Reports

Bugs and feature requests go to [Issues](https://github.com/SebastianBoehler/limitless-cpp-client/issues).
Security reports follow [SECURITY.md](SECURITY.md).
