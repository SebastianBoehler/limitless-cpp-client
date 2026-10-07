#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
cmake -S . -B build -DLIMITLESS_CLIENT_BUILD_EXAMPLES=ON -DLIMITLESS_CLIENT_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
