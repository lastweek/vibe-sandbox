#!/usr/bin/env bash
# Dedicated benchmark for container start latency with high iteration counts.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/perf/common.sh
source "$SCRIPT_DIR/common.sh"

TEST_NAME="ns-runtime-start-latency"
WARMUP_RUNS="${WARMUP_RUNS:-20}"
START_RUNS="${START_RUNS:-500}"
VERIFY_RUNNING="${VERIFY_RUNNING:-1}"
PROGRESS_STEP="${PROGRESS_STEP:-50}"

SAMPLES_FILE="$(mktemp -t ns-start-latency.XXXXXX)"
trap 'rm -f "$SAMPLES_FILE"' EXIT

calc_stats() {
    awk '
    BEGIN {
        sum = 0
        sum_sq = 0
        min = 999999999
        max = 0
        count = 0
    }
    {
        val = $1 / 1000.0
        sum += val
        sum_sq += val * val
        if (val < min) min = val
        if (val > max) max = val
        count++
    }
    END {
        if (count > 0) {
            mean = sum / count
            variance = (sum_sq / count) - (mean * mean)
            if (variance < 0) variance = 0
            stddev = sqrt(variance)
            printf "  Count:      %d\n", count
            printf "  Min:        %.3f ms\n", min
            printf "  Max:        %.3f ms\n", max
            printf "  Mean:       %.3f ms\n", mean
            printf "  Std Dev:    %.3f ms\n", stddev
        }
    }
    '
}

calc_percentiles() {
    sort -n | awk '
    BEGIN { count = 0 }
    { vals[count++] = $1 }
    END {
        if (count > 0) {
            p50_idx = int((count - 1) * 0.50)
            p95_idx = int((count - 1) * 0.95)
            p99_idx = int((count - 1) * 0.99)
            printf "  p50:        %.3f ms\n", vals[p50_idx] / 1000.0
            printf "  p95:        %.3f ms\n", vals[p95_idx] / 1000.0
            printf "  p99:        %.3f ms\n", vals[p99_idx] / 1000.0
        }
    }
    '
}

calc_trimmed_mean() {
    sort -n | awk '
    BEGIN { count = 0 }
    { vals[count++] = $1 }
    END {
        if (count == 0) {
            exit
        }
        drop = int(count * 0.05)
        start = drop
        stop = count - drop - 1
        if (stop < start) {
            start = 0
            stop = count - 1
        }
        sum = 0
        n = 0
        for (i = start; i <= stop; i++) {
            sum += vals[i]
            n++
        }
        if (n > 0) {
            printf "  Mean(5%% trim): %.3f ms\n", (sum / n) / 1000.0
        }
    }
    '
}

cleanup_container() {
    local id="$1"
    "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" delete "$id" >/dev/null 2>&1 || true
}

perf_header "nano-sandbox Start Latency Benchmark"
echo "Configuration: warmup=${WARMUP_RUNS}, runs=${START_RUNS}, verify_running=${VERIFY_RUNNING}"

perf_require_env

perf_section "Warmup"
for i in $(seq 1 "$WARMUP_RUNS"); do
    id="${TEST_NAME}-warmup-${i}-$$"
    "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" create --bundle="$NS_TEST_BUNDLE" "$id" >/dev/null 2>&1 || true
    "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" start "$id" >/dev/null 2>&1 || true
    cleanup_container "$id"
done
echo "Warmup complete"
echo

perf_section "Measured start() runs"
echo -n "Progress: "
success=0
failed=0

for i in $(seq 1 "$START_RUNS"); do
    id="${TEST_NAME}-run-${i}-$$"
    if ! "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" create --bundle="$NS_TEST_BUNDLE" "$id" >/dev/null 2>&1; then
        failed=$((failed + 1))
        cleanup_container "$id"
        perf_progress_dot "$i" "$PROGRESS_STEP"
        continue
    fi

    set +e
    start_us=$(perf_time_us "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" start "$id")
    start_rc=$?
    set -e
    if [ "$start_rc" -ne 0 ]; then
        failed=$((failed + 1))
        cleanup_container "$id"
        perf_progress_dot "$i" "$PROGRESS_STEP"
        continue
    fi

    if [ "$VERIFY_RUNNING" = "1" ]; then
        state="$("${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" state "$id" 2>/dev/null || true)"
        if [ "$state" != "running" ]; then
            failed=$((failed + 1))
            cleanup_container "$id"
            perf_progress_dot "$i" "$PROGRESS_STEP"
            continue
        fi
    fi

    echo "$start_us" >>"$SAMPLES_FILE"
    success=$((success + 1))
    cleanup_container "$id"
    perf_progress_dot "$i" "$PROGRESS_STEP"
done

echo
echo

if [ "$success" -eq 0 ]; then
    nk_die "no successful start samples collected (failed=${failed})"
fi

echo -e "${GREEN}Start Latency Results:${NC}"
cat "$SAMPLES_FILE" | calc_stats
cat "$SAMPLES_FILE" | calc_percentiles
cat "$SAMPLES_FILE" | calc_trimmed_mean
echo "  Successful:  ${success}"
echo "  Failed:      ${failed}"
echo

mean_us="$(awk '{sum+=$1} END {if (NR>0) print sum/NR; else print 0}' "$SAMPLES_FILE")"
echo -e "${GREEN}Average start latency over ${success} successful runs: $(perf_ms_from_us "$mean_us") ms${NC}"
