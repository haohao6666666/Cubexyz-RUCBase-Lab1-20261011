#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake --preset release
cmake --build --preset release -j 4 --target lab1_fresh_benchmark
mkdir -p evidence/reproduced
benchmark="$PWD/build/release/bin/lab1_fresh_benchmark"
scratch=$(mktemp -d)
trap 'rmdir "$scratch"' EXIT
(cd "$scratch" && "$benchmark") > evidence/reproduced/performance.csv
