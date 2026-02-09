#!/usr/bin/env bash
# Measure latency of individual runtime operations.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/perf/common.sh
source "$SCRIPT_DIR/common.sh"

TEST_NAME="ns-runtime-api-perf"
WARMUP_RUNS="${WARMUP_RUNS:-5}"
TEST_RUNS="${TEST_RUNS:-100}"

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
            printf "  Throughput: %.0f ops/sec\n", 1000.0 / mean
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
            p50_idx = int(count * 0.50)
            p95_idx = int(count * 0.95)
            p99_idx = int(count * 0.99)
            printf "  p50:        %.3f ms\n", vals[p50_idx] / 1000.0
            printf "  p95:        %.3f ms\n", vals[p95_idx] / 1000.0
            printf "  p99:        %.3f ms\n", vals[p99_idx] / 1000.0
        }
    }
    '
}

run_series() {
    local label="$1"
    local op="$2"
    local -n out_ref="$3"

    perf_section "$label"
    echo

    out_ref=()
    for i in $(seq 1 "$TEST_RUNS"); do
        id="${TEST_NAME}-${op}-${i}-$$"

        case "$op" in
            create)
                t=$(perf_time_us "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" create --bundle="$NS_TEST_BUNDLE" "$id")
                out_ref+=("$t")
                "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" delete "$id" >/dev/null 2>&1 || true
                ;;
            start)
                "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" create --bundle="$NS_TEST_BUNDLE" "$id" >/dev/null 2>&1 || true
                t=$(perf_time_us "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" start "$id")
                out_ref+=("$t")
                "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" delete "$id" >/dev/null 2>&1 || true
                ;;
            delete)
                "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" create --bundle="$NS_TEST_BUNDLE" "$id" >/dev/null 2>&1 || true
                "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" start "$id" >/dev/null 2>&1 || true
                t=$(perf_time_us "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" delete "$id")
                out_ref+=("$t")
                ;;
            *)
                nk_die "unknown op: $op"
                ;;
        esac

        perf_progress_dot "$i" 20
    done

    echo
    echo
    echo -e "${GREEN}${label} Results:${NC}"
    printf '%s\n' "${out_ref[@]}" | calc_stats
    printf '%s\n' "${out_ref[@]}" | calc_percentiles
    echo
}

perf_header "nano-sandbox API Latency Performance"
echo "Configuration: warmup=${WARMUP_RUNS}, runs=${TEST_RUNS}"

perf_require_env

perf_section "Warmup"
for i in $(seq 1 "$WARMUP_RUNS"); do
    id="${TEST_NAME}-warmup-${i}-$$"
    "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" create --bundle="$NS_TEST_BUNDLE" "$id" >/dev/null 2>&1 || true
    "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" delete "$id" >/dev/null 2>&1 || true
done
echo "Warmup complete"
echo

CREATE_TIMES=()
START_TIMES=()
DELETE_TIMES=()
STATE_TIMES=()

run_series "CREATE Operation Latency" "create" CREATE_TIMES
run_series "START Operation Latency" "start" START_TIMES
run_series "DELETE Operation Latency" "delete" DELETE_TIMES

perf_section "STATE Query Latency"
state_id="${TEST_NAME}-state-$$"
"${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" create --bundle="$NS_TEST_BUNDLE" "$state_id" >/dev/null 2>&1 || true
for i in $(seq 1 "$TEST_RUNS"); do
    t=$(perf_time_us "${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" state "$state_id")
    STATE_TIMES+=("$t")
    perf_progress_dot "$i" 25
done
echo
echo
"${PERF_SUDO[@]}" "${PERF_CMD_PREFIX[@]}" "$NS_RUNTIME_BIN" delete "$state_id" >/dev/null 2>&1 || true

echo -e "${GREEN}STATE Query Results:${NC}"
printf '%s\n' "${STATE_TIMES[@]}" | calc_stats
printf '%s\n' "${STATE_TIMES[@]}" | calc_percentiles
echo

perf_header "Performance Summary"
create_mean_us=$(printf '%s\n' "${CREATE_TIMES[@]}" | awk '{sum+=$1} END {print sum/NR}')
start_mean_us=$(printf '%s\n' "${START_TIMES[@]}" | awk '{sum+=$1} END {print sum/NR}')
delete_mean_us=$(printf '%s\n' "${DELETE_TIMES[@]}" | awk '{sum+=$1} END {print sum/NR}')
state_mean_us=$(printf '%s\n' "${STATE_TIMES[@]}" | awk '{sum+=$1} END {print sum/NR}')

echo "CREATE: $(perf_ms_from_us "$create_mean_us") ms"
echo "START:  $(perf_ms_from_us "$start_mean_us") ms"
echo "DELETE: $(perf_ms_from_us "$delete_mean_us") ms"
echo "STATE:  $(perf_ms_from_us "$state_mean_us") ms"
