#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/host
common=(-std=c++17 -O1 -g -DXU4_PICO -fsanitize=address,undefined
    -fno-sanitize-recover=all -fno-pie -no-pie -Itests/diag-test -Iplatform/native)
"${CXX:-g++}" "${common[@]}" tests/prekey-test.cpp -o build/host/prekey
build/host/prekey
"${CXX:-g++}" "${common[@]}" -ffunction-sections -fdata-sections -Wl,--gc-sections \
    tests/diag-test/test.cpp tests/diag-test/timeline-wrapper.cpp -o build/host/diagnostics
build/host/diagnostics
