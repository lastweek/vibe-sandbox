# Performance Tests

Performance benchmarks for `ns-runtime`.

## Entry Points

Recommended:

```bash
./scripts/bench.sh                # all benchmarks
./scripts/bench.sh micro          # quick lifecycle check
./scripts/bench.sh latency        # API latency stats
./scripts/bench.sh start          # dedicated start() latency
./scripts/bench.sh throughput     # throughput/stress
```

Direct scripts (advanced use):

```bash
./scripts/perf/test_microbench.sh
./scripts/perf/test_api_latency.sh
./scripts/perf/test_start_latency.sh
./scripts/perf/test_throughput.sh
```

## Prerequisites

Run on Linux with sudo/root access:

```bash
make install
```

This installs:
- runtime: `/usr/local/bin/ns-runtime`
- bundle: `/usr/local/share/nano-sandbox/bundle`

## Environment Variables

- `NS_RUNTIME_BIN` override runtime path
- `NS_TEST_BUNDLE` override bundle path
- `NS_RUN_DIR` override state directory
- `ITERATIONS`, `TEST_RUNS`, `START_RUNS`, `WARMUP_RUNS`, `STRESS_COUNT`, `QUERY_COUNT` tune workload size
- `VERIFY_RUNNING=1` enforces state validation for each `start` sample in start-latency benchmark
- Benchmarks disable runtime logging via `NK_LOG_ENABLED=0` to reduce noise and overhead

## Notes

- Benchmarks intentionally favor readability over strict scientific methodology.
- Use isolated hosts/VMs and repeat runs if you need stable regressions tracking.
