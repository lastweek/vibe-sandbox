#!/usr/bin/env bash
# Performance benchmark runner for nano-sandbox.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/lib/common.sh
source "$SCRIPT_DIR/lib/common.sh"

usage() {
    cat <<USAGE
Usage: ./scripts/bench.sh [all|latency|start|throughput|micro]

Benchmarks:
  all         Run micro, latency, and throughput benchmarks
  latency     Run API latency benchmark
  start       Run high-iteration start-latency benchmark
  throughput  Run throughput/stress benchmark
  micro       Run quick microbenchmark
USAGE
}

bench="${1:-all}"
case "$bench" in
    -h|--help)
        usage
        exit 0
        ;;
    all|latency|start|throughput|micro)
        ;;
    *)
        nk_usage_error "unknown benchmark: $bench"
        ;;
esac

PERF_DIR="$NK_PERF_DIR"

nk_print_header "nano-sandbox Performance Benchmarks"
nk_info "Runtime:   $NS_RUNTIME_BIN"
nk_info "Bundle:    $NS_TEST_BUNDLE"
nk_info "Benchmark: $bench"

nk_require_installed_runtime
nk_require_installed_bundle
nk_prepare_run_dir

case "$bench" in
    latency)
        nk_run_named_script "$PERF_DIR/test_api_latency.sh" "API Latency"
        ;;
    start)
        nk_run_named_script "$PERF_DIR/test_start_latency.sh" "Start Latency"
        ;;
    throughput)
        nk_run_named_script "$PERF_DIR/test_throughput.sh" "Throughput & Stress"
        ;;
    micro)
        nk_run_named_script "$PERF_DIR/test_microbench.sh" "Microbenchmark"
        ;;
    all)
        nk_run_named_script "$PERF_DIR/test_microbench.sh" "Microbenchmark"
        nk_run_named_script "$PERF_DIR/test_api_latency.sh" "API Latency"
        nk_run_named_script "$PERF_DIR/test_start_latency.sh" "Start Latency"
        nk_run_named_script "$PERF_DIR/test_throughput.sh" "Throughput & Stress"
        ;;
esac

nk_info "=== All Benchmarks Complete ==="
nk_info "Details: $PERF_DIR/README.md"
