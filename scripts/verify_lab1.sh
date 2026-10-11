#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mode="${1:-debug}"
cmake --preset "$mode"
cmake --build --preset "$mode" -j 4 --target lab1_disk_manager_test lab1_lru_replacer_test lab1_buffer_pool_manager_test lab1_record_manager_test lab1_page_guard_test lab1_record_types_test lab1_record_layout_test lab1_rm_page_guard_test lab1_fresh_boundary_test
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir "build/$mode" -L lab1 --output-on-failure
