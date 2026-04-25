#!/usr/bin/env bash
# rebuilds in tsan mode and runs the test suite. tsan-specific build dir so
# you can keep an asan dir running in parallel.

set -euo pipefail

cd "$(dirname "$0")/.."

cmake -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DSSIKV_TSAN=ON -DSSIKV_ASAN=OFF
cmake --build build-tsan -j

# tsan exit code is 66 on race; ctest returns 1 if subprocess aborted, so
# either way nonzero exit propagates.
ctest --test-dir build-tsan --output-on-failure
echo "tsan: clean"
