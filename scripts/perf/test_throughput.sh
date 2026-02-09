#!/usr/bin/env bash
# Throughput and stress benchmark.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/perf/common.sh
source "$SCRIPT_DIR/common.sh"

TEST_NAME="ns-runtime-throughput"
ITERATIONS="${ITERATIONS:-100}"
STRESS_COUNT="${STRESS_COUNT:-300}"
QUERY_COUNT="${QUERY_COUNT:-1000}"
CONCURRENT_COUNTS=(10 50 100)

cleanup() {
    echo -e "\n${YELLOW}Cleaning up stress containers...${NC}"
    for i in $(seq 1 "$STRESS_COUNT"); do
        "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" delete "${TEST_NAME}-stress-${i}-$$" >/dev/null 2>&1 || true
    done
    echo -e "${GREEN}✓ Cleanup complete${NC}"
}
trap cleanup EXIT

ns_elapsed_us() {
    local start_ns="$1"
    local end_ns="$2"
    echo $(((end_ns - start_ns) / 1000))
}

perf_header "nano-sandbox Throughput & Stress"
perf_require_env

perf_section "Test 1: Sequential Lifecycle Throughput"
echo "Running ${ITERATIONS} create/start/delete cycles..."

seq_start=$(date +%s%N)
for i in $(seq 1 "$ITERATIONS"); do
    id="${TEST_NAME}-seq-${i}-$$"
    "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" create --bundle="$NS_TEST_BUNDLE" "$id" >/dev/null 2>&1
    "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" start "$id" >/dev/null 2>&1
    "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" delete "$id" >/dev/null 2>&1 || true
    perf_progress_dot "$i" 10
done
seq_end=$(date +%s%N)

echo
seq_total_us=$(ns_elapsed_us "$seq_start" "$seq_end")
seq_total_s=$(echo "scale=3; $seq_total_us / 1000000" | bc)
seq_avg_us=$((seq_total_us / ITERATIONS))
seq_avg_ms=$(perf_ms_from_us "$seq_avg_us")
seq_tput=$(echo "scale=1; $ITERATIONS / $seq_total_s" | bc)

echo "  Total time:  ${seq_total_s} s"
echo "  Avg latency: ${seq_avg_ms} ms/container"
echo "  Throughput:  ${seq_tput} containers/sec"

echo
perf_section "Test 2: Concurrent Lifecycle Throughput"

for n in "${CONCURRENT_COUNTS[@]}"; do
    echo "Concurrent count: ${n}"
    start_ns=$(date +%s%N)

    for i in $(seq 1 "$n"); do
        id="${TEST_NAME}-conc-${n}-${i}-$$"
        (
            "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" create --bundle="$NS_TEST_BUNDLE" "$id" >/dev/null 2>&1 &&
            "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" start "$id" >/dev/null 2>&1 &&
            "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" delete "$id" >/dev/null 2>&1
        ) &
    done
    wait

    end_ns=$(date +%s%N)
    total_us=$(ns_elapsed_us "$start_ns" "$end_ns")
    total_s=$(echo "scale=3; $total_us / 1000000" | bc)
    avg_ms=$(echo "scale=3; $total_us / $n / 1000" | bc)
    tput=$(echo "scale=1; $n / $total_s" | bc)

    echo "  Total time:  ${total_s} s"
    echo "  Avg latency: ${avg_ms} ms"
    echo "  Throughput:  ${tput} ops/sec"
    echo

done

perf_section "Test 3: Create-only Stress"
echo "Creating ${STRESS_COUNT} containers..."

stress_start=$(date +%s%N)
pids=()
for i in $(seq 1 "$STRESS_COUNT"); do
    id="${TEST_NAME}-stress-${i}-$$"
    "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" create --bundle="$NS_TEST_BUNDLE" "$id" >/dev/null 2>&1 &
    pids+=("$!")
    perf_progress_dot "$i" 50
done

echo
for pid in "${pids[@]}"; do
    wait "$pid" || true
done
stress_end=$(date +%s%N)

stress_total_us=$(ns_elapsed_us "$stress_start" "$stress_end")
stress_total_s=$(echo "scale=3; $stress_total_us / 1000000" | bc)
stress_avg_us=$((stress_total_us / STRESS_COUNT))
stress_avg_ms=$(perf_ms_from_us "$stress_avg_us")
stress_tput=$(echo "scale=1; $STRESS_COUNT / $stress_total_s" | bc)

echo "  Total time:      ${stress_total_s} s"
echo "  Avg create time: ${stress_avg_ms} ms"
echo "  Throughput:      ${stress_tput} containers/sec"

created_count=$(find "$NS_RUN_DIR" -maxdepth 1 -type d -name "${TEST_NAME}-stress-*-$$" | wc -l | tr -d ' ')
echo "  State dirs:      ${created_count}"

echo
perf_section "Test 4: State Query Under Load"
query_id="${TEST_NAME}-stress-1-$$"

query_start=$(date +%s%N)
for i in $(seq 1 "$QUERY_COUNT"); do
    "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" state "$query_id" >/dev/null 2>&1 || true
    perf_progress_dot "$i" 100
done
query_end=$(date +%s%N)

echo
query_total_us=$(ns_elapsed_us "$query_start" "$query_end")
query_total_s=$(echo "scale=3; $query_total_us / 1000000" | bc)
query_avg_us=$((query_total_us / QUERY_COUNT))
query_avg_ms=$(perf_ms_from_us "$query_avg_us")
query_qps=$(echo "scale=1; $QUERY_COUNT / $query_total_s" | bc)

echo "  Total time:  ${query_total_s} s"
echo "  Avg latency: ${query_avg_ms} ms"
echo "  Query rate:  ${query_qps} qps"

echo
perf_header "Stress Summary"
echo "  Sequential throughput: ${seq_tput} containers/sec"
echo "  Stress create rate:    ${stress_tput} containers/sec"
echo "  State query rate:      ${query_qps} qps"
