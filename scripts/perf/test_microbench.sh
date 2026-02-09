#!/usr/bin/env bash
# Quick lifecycle microbenchmark.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/perf/common.sh
source "$SCRIPT_DIR/common.sh"

ITERATIONS="${ITERATIONS:-20}"
TEST_PREFIX="nk-micro"

perf_header "nano-sandbox Microbenchmark"
echo "Quick lifecycle test (${ITERATIONS} iterations)"

perf_require_env

total_us=0
for i in $(seq 1 "$ITERATIONS"); do
    id="${TEST_PREFIX}-${i}-$$"

    create_us=$(perf_time_us "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" create --bundle="$NS_TEST_BUNDLE" "$id")
    start_us=$(perf_time_us "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" start "$id")
    delete_us=$(perf_time_us "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" delete "$id")

    total_us=$((total_us + create_us + start_us + delete_us))
    perf_progress_dot "$i" 5
done

echo
echo

avg_us=$((total_us / ITERATIONS))
avg_ms=$(perf_ms_from_us "$avg_us")
total_s=$(echo "scale=3; $total_us / 1000000" | bc)
throughput=$(echo "scale=1; 1000 / $avg_ms" | bc)

echo -e "${GREEN}Results:${NC}"
echo "  Total time:    ${total_s} s"
echo "  Iterations:    ${ITERATIONS}"
echo "  Avg lifecycle: ${avg_ms} ms"
echo "  Throughput:    ${throughput} containers/second"

echo
if [ "$avg_us" -lt 20000 ]; then
    echo -e "${GREEN}✓ Excellent (< 20ms avg lifecycle)${NC}"
elif [ "$avg_us" -lt 50000 ]; then
    echo -e "${GREEN}✓ Good (< 50ms avg lifecycle)${NC}"
elif [ "$avg_us" -lt 100000 ]; then
    echo -e "${YELLOW}⚠ Acceptable (< 100ms avg lifecycle)${NC}"
else
    echo -e "${RED}✗ Slow (> 100ms avg lifecycle)${NC}"
fi
