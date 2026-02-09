# Execution Flow

## Command Summary

- `create`: parse + validate OCI bundle, persist metadata, no process spawned.
- `start`: load created container, build execution context, clone child, start process.
- `run`: `create` + `start` in one command; attached by default.
- `exec`: enter namespaces of a running container (`nsenter`), interactive by default.
- `delete`: stop running process (if needed), cleanup cgroup/state.
- `state`: query persisted state; non-zero exit for not found.

## Command Dispatch Map

```mermaid
flowchart TD
    A[CLI args parsed] --> B{command}
    B -->|create| C[nk_container_create]
    B -->|start| D[nk_container_start]
    B -->|run| E[nk_container_run]
    B -->|exec| F[nk_container_resume]
    B -->|delete| G[nk_container_delete]
    B -->|state| I[nk_container_state]
    B -->|help/version| H[print help/version]

    E --> C
    E --> D
```

## `create` Flow

1. Parse args and validate command shape.
2. Ensure state root exists.
3. Reject duplicate container id.
4. Load OCI spec from bundle and validate it.
5. Construct in-memory `nk_container_t` in `CREATED` state.
6. Persist state to disk.

Output expectation:
- Logs include bundle/root details and final `Status: created`.

## `start` Flow

1. Load container state by id.
2. Enforce state precondition (`CREATED`).
3. Load OCI spec and log parsed startup summary.
4. Build `nk_container_ctx_t`:
   - rootfs path
   - namespace list
   - process args/env/cwd
   - cgroup config
5. Call `nk_container_exec()`.
6. On success:
   - persist `RUNNING` state with PID
   - detached mode returns immediately
   - attached mode waits for child exit, then persists `STOPPED`

```mermaid
flowchart TD
    S1[load saved container state] --> S2{state == CREATED?}
    S2 -->|no| Sx[fail]
    S2 -->|yes| S3[load + validate OCI spec]
    S3 --> S4[build nk_container_ctx]
    S4 --> S5[nk_container_exec]
    S5 --> S6{exec startup ok?}
    S6 -->|no| Sx
    S6 -->|yes| S7[save RUNNING + pid]
    S7 --> S8{attach mode?}
    S8 -->|no| S9[return success detached]
    S8 -->|yes| S10[wait child exit]
    S10 --> S11[save STOPPED + exit code]
```

## `run` Flow

1. Execute `create` flow.
2. Execute `start` flow.
3. If `--rm` set and execution completed, delete container metadata.

Return semantics:
- Attached mode returns container process exit code.
- Detached mode returns runtime operation status.

## `exec` Flow

Design reference: for rationale, invariants, and failure boundaries, see [`exec-design.md`](exec-design.md).

1. Load container state.
2. Enforce state precondition (`RUNNING` + live init PID).
3. Require host `/proc` to be mounted (`nsenter` depends on `/proc/<pid>/ns/*`).
4. Spawn `nsenter --target <pid> --mount --uts --ipc --net --pid`.
5. Execute:
   - interactive mode: `/bin/sh`
   - command mode: `/bin/sh -lc "<cmd>"`
6. Return entered-command exit code.
7. If init PID is gone, persist `STOPPED` state.

```mermaid
flowchart TD
    R1[load saved container state] --> R2{state == RUNNING && pid > 0?}
    R2 -->|no| Rx[fail]
    R2 -->|yes| R3{init pid alive?}
    R3 -->|no| R4[mark STOPPED]
    R4 --> Rx
    R3 -->|yes| R5{host /proc available?}
    R5 -->|no| Rx
    R5 -->|yes| R6[fork + exec nsenter]
    R6 --> R7[wait child]
    R7 --> R8[return child exit code]
```

## Child Process Startup (`nk_container_exec`)

1. Parent computes namespace clone flags.
2. Parent allocates child stack and opens sync pipe.
3. `clone()` starts child function.
4. Child sequence:
   - switch log role to `CHILD`
   - configure hostname (if UTS ns requested)
   - setup rootfs/mounts
   - apply capability/resource steps
   - signal parent readiness over pipe
   - `execve()` configured process
5. Parent waits for readiness byte:
   - ready -> continue + state update
   - error/EOF -> fail startup

```mermaid
sequenceDiagram
    participant P as Parent (ns-runtime)
    participant C as Child (clone target)
    participant K as Kernel

    P->>P: build clone flags + create sync pipe
    P->>K: clone(child_fn, flags)
    K-->>C: start child context
    C->>C: set role CHILD + setup rootfs/mounts
    C->>C: setup hostname/caps/rlimits
    C-->>P: write READY byte on sync pipe
    P->>P: read sync byte
    alt READY
      P->>P: persist RUNNING + pid
      C->>K: execve(process args)
    else ERROR/EOF
      P->>P: startup fail path
    end
```

## `delete` Flow

1. Load state.
2. If running, send `SIGTERM`, then `SIGKILL` fallback.
3. Cleanup cgroup path.
4. Remove persisted state.
5. Report `Status: deleted`.

## `state` Flow

1. Load state file by id.
2. Print state string (`created`, `running`, `stopped`, `paused`).
3. If missing/invalid, print `unknown` and exit non-zero.

## Failure Visibility Model

Failure classes are surfaced in three layers:
1. Runtime return code (`main` command exit status)
2. Structured logs (parent/child role + file:line)
3. Integration tests that parse outputs for startup failure patterns

## Lifecycle State Transitions

```mermaid
stateDiagram-v2
    [*] --> CREATED: create
    CREATED --> RUNNING: start or run
    RUNNING --> RUNNING: exec
    RUNNING --> STOPPED: attached wait exit
    CREATED --> DELETED: delete
    RUNNING --> DELETED: delete
    STOPPED --> DELETED: delete
    DELETED --> [*]
```
