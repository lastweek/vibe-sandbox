# Command Execution Reference

## Overview

nano-sandbox implements the standard OCI runtime lifecycle commands. This document provides detailed execution flows for each command, including error handling, state transitions, and inter-process communication.

## Command Summary

| Command | Purpose | State Change | Process Created |
|---------|---------|--------------|-----------------|
| `create` | Parse bundle, validate, persist metadata | → CREATED | No |
| `start` | Start container process | CREATED → RUNNING | Yes |
| `run` | Create + start in one step | → CREATED → RUNNING | Yes |
| `exec` | Execute command in running container | None | Yes (temporary) |
| `delete` | Stop and cleanup | → DELETED | No |
| `state` | Query container status | None | No |

## Command Dispatch

```mermaid
flowchart TD
    START[CLI: nk-runtime <cmd> <args>] --> PARSE[Parse Arguments]
    PARSE --> VALIDATE{Validate Command}

    VALIDATE -->|create| CREATE[nk_container_create]
    VALIDATE -->|start| START[nk_container_start]
    VALIDATE -->|run| RUN[nk_container_run]
    VALIDATE -->|exec| EXEC[nk_container_exec]
    VALIDATE -->|delete| DELETE[nk_container_delete]
    VALIDATE -->|state| STATE[nk_container_state]

    CREATE --> OUT1[Return to shell]
    START --> OUT2[Return to shell]
    RUN --> OUT3[Return to shell]
    EXEC --> OUT4[Return to shell]
    DELETE --> OUT5[Return to shell]
    STATE --> OUT6[Print state]

    style OUT1 fill:#e1f5e1
    style OUT2 fill:#e1f5e1
    style OUT3 fill:#e1f5e1
    style OUT4 fill:#e1f5e1
    style OUT5 fill:#e1f5e1
    style OUT6 fill:#e1f5e1
```

## 1. CREATE Command

### Syntax
```bash
nk-runtime create --bundle=<path> <container-id>
```

### Purpose
Parse and validate OCI bundle, create container metadata, persist to disk. Does **NOT** start any process.

### Execution Flow

```mermaid
sequenceDiagram
    participant CLI as CLI
    participant ST as State Manager
    participant OCI as OCI Parser
    participant FS as Filesystem

    CLI->>ST: Ensure state directory exists
    ST->>FS: mkdir -p /run/nano-sandbox

    CLI->>ST: Check if container exists
    ST->>FS: stat state.json
    alt Container exists
        FS-->>CLI: Error: Already exists
        CLI-->>Shell: Exit 1
    end

    CLI->>OCI: Load config.json from bundle
    OCI->>FS: Read bundle/config.json
    OCI->>OCI: Parse JSON
    OCI->>OCI: Validate required fields

    alt Validation fails
        OCI-->>CLI: Error: Invalid spec
        CLI-->>Shell: Exit 1
    end

    CLI->>CLI: Create nk_container_t
    CLI->>CLI: Set state = CREATED
    CLI->>ST: Save container state
    ST->>FS: Write state.json

    CLI-->>Shell: Status: created
    CLI-->>Shell: Print bundle/root details
```

### State Transition
```
INVALID → CREATED
```

### Key Functions
- `ensure_state_dir()` - Create state root directory
- `nk_state_exists()` - Check for duplicate container ID
- `nk_oci_spec_load()` - Parse config.json
- `nk_oci_spec_validate()` - Validate required sections
- `nk_state_save()` - Persist container metadata

### Output Example
```
[08:39:59.725] [INFO] Creating container 'test1' (mode: container)
[08:39:59.732] [INFO] [1] Loading OCI spec from bundle
[08:39:59.740] [INFO] [2] Validating OCI spec
[08:39:59.745] [INFO] [3] Creating container metadata
[08:39:59.750] [INFO] [4] Saving container state to disk
  Bundle: /path/to/bundle
  Root: rootfs
  Status: created
```

### Error Cases
- Container ID already exists
- Bundle directory not found
- config.json not found or invalid
- Missing required OCI spec fields
- State directory not writable

---

## 2. START Command

### Syntax
```bash
nk-runtime start [--attach|--detach] <container-id>
```

### Purpose
Load created container, spawn isolated process, transition to RUNNING state.

### Execution Flow

```mermaid
sequenceDiagram
    participant CLI as CLI
    participant ST as State Manager
    participant PARENT as Parent Process
    participant CHILD as Child Process
    participant KERNEL as Kernel

    CLI->>ST: Load container state
    ST->>ST: Read state.json

    alt State not found
        ST-->>CLI: Error: Container not found
        CLI-->>Shell: Exit 1
    end

    alt State != CREATED
        ST-->>CLI: Error: Invalid state for start
        CLI-->>Shell: Exit 1
    end

    CLI->>CLI: Load OCI spec
    CLI->>CLI: Build execution context
    Note over CLI: rootfs, namespaces,<br/>process args, cgroups

    CLI->>PARENT: Call nk_container_exec()

    PARENT->>PARENT: Compute clone flags
    PARENT->>PARENT: Allocate child stack
    PARENT->>PARENT: Create sync pipe

    PARENT->>KERNEL: clone(child_fn, flags)
    KERNEL-->>CHILD: Start child in isolated context

    par Child Setup
        CHILD->>CHILD: Set log role = CHILD
        CHILD->>CHILD: Set hostname (if UTS ns)
        CHILD->>CHILD: Setup rootfs
        CHILD->>CHILD: Mount proc/sys/dev
        CHILD->>CHILD: Join cgroup
        CHILD->>CHILD: Drop capabilities
        CHILD->>PARENT: Write READY byte
    and Parent Wait
        PARENT->>PARENT: Read sync pipe
    end

    alt Child sent ERROR
        PARENT->>ST: Don't save RUNNING state
        PARENT-->>CLI: Error: Child setup failed
        CLI-->>Shell: Exit 1
    end

    PARENT->>ST: Save RUNNING state + PID
    ST->>ST: Write state.json

    alt --detach mode (default)
        PARENT-->>CLI: Return immediately
    else --attach mode
        PARENT->>PARENT: Wait for child to exit
        CHILD->>KERNEL: execve(process args)
        Note over CHILD: Becomes PID 1 in container
        KERNEL-->>PARENT: Child exit signal
        PARENT->>ST: Save STOPPED + exit code
        PARENT-->>CLI: Return child exit code
    end
```

### State Transition
```
CREATED → RUNNING → STOPPED (on exit)
```

### Key Functions
- `nk_container_exec()` - Main process orchestration
- `container_child_fn()` - Child process setup
- `setup_namespaces()` - Configure hostname
- `setup_rootfs()` - pivot_root and mounts
- `join_cgroup()` - Add to cgroup
- `drop_capabilities()` - Reduce privileges

### Output Example
```
[08:40:01.123] [INFO] Starting container 'test1'
[08:40:01.125] [INFO] [1] Loading container state
[08:40:01.128] [INFO] [2] Loading OCI spec
[08:40:01.130] [INFO] [3] Setting up namespaces
[08:40:01.132] [DEBUG] Clone flags: 0x3e0f00 (pid,net,ipc,uts,mount)
[08:40:01.135] [INFO] [4] Spawning container process
[08:40:01.140] [INFO] Container started with PID 12345
```

### Error Cases
- Container not found
- Container already running
- Child process creation failed
- Rootfs setup failed
- execve() failed

---

## 3. RUN Command

### Syntax
```bash
nk-runtime run [--attach|--detach] [--rm] --bundle=<path> <container-id>
```

### Purpose
Create + start in one command. Combines both operations with `--rm` option for automatic cleanup.

### Execution Flow

```mermaid
flowchart TD
    START[run command] --> CREATE[Execute create flow]
    CREATE --> CREATE_OK{Create success?}

    CREATE_OK -->|no| ERROR[Return error]
    CREATE_OK -->|yes| START[Execute start flow]

    START --> START_OK{Start success?}
    START_OK -->|no| DELETE1[Cleanup via delete]
    DELETE_OK --> ERROR

    START_OK -->|yes| MODE{Attach mode?}

    MODE -->|attach| WAIT[Wait for child]
    WAIT --> EXIT[Child exits]
    EXIT --> RM{--rm flag?}
    RM -->|yes| DELETE2[Delete container]
    RM -->|no| SUCCESS[Return exit code]
    DELETE2 --> SUCCESS

    MODE -->|detach| SUCCESS

    style SUCCESS fill:#e1f5e1
    style ERROR fill:#f5e1e1
```

### Key Differences from create+start

| Feature | `run` | `create + start` |
|---------|-------|------------------|
| Container state | Must not exist | Must be CREATED |
| Attach mode | Default (user sees output) | Default detached |
| `--rm` option | Yes | No (must delete manually) |
| Exit code | Container exit code | Runtime operation status |

### Use Cases

**Attached run (default):**
```bash
nk-runtime run --bundle=/path/to/bundle mycontainer
# Creates, starts, waits for exit, returns container exit code
```

**Detached run:**
```bash
nk-runtime run --detach --bundle=/path/to/bundle mycontainer
# Creates, starts, returns immediately
```

**Auto-cleanup:**
```bash
nk-runtime run --rm --bundle=/path/to/bundle temp
# Creates, starts, waits, deletes on exit
```

---

## 4. EXEC Command

### Syntax
```bash
nk-runtime exec [--exec "<command>"] <container-id>
```

### Purpose
Enter namespaces of a **running** container and execute a command. Interactive by default.

### Execution Flow

```mermaid
sequenceDiagram
    participant CLI as CLI
    participant ST as State Manager
    participant PARENT as Parent (ns-runtime)
    participant NSENTER as nsenter Process
    participant CONT as Container PID 1

    CLI->>ST: Load container state
    ST->>ST: Read state.json

    alt State not found
        ST-->>CLI: Error: Container not found
        CLI-->>Shell: Exit 1
    end

    alt State != RUNNING
        ST-->>CLI: Error: Container not running
        CLI-->>Shell: Exit 1
    end

    CLI->>CLI: Check if init PID alive
    alt PID not alive
        CLI->>ST: Mark as STOPPED
        CLI-->>Shell: Exit 1
    end

    CLI->>CLI: Check /proc mounted (nsenter requirement)
    alt /proc not available
        CLI-->>Shell: Error: /proc not mounted
        CLI-->>Shell: Exit 1
    end

    CLI->>PARENT: Fork nsenter process

    alt --exec mode (command specified)
        PARENT->>NSENTER: exec nsenter --target <pid> --mount --uts --ipc --net --pid -- /bin/sh -lc "<command>"
    else interactive mode (default)
        PARENT->>NSENTER: exec nsenter --target <pid> --mount --uts --ipc --net --pid -- /bin/sh
    end

    NSENTER->>NSENTER: Join container namespaces
    NSENTER->>CONT: Execute command/shell
    CONT-->>NSENTER: Command output
    NSENTER-->>CLI: Return exit code
```

### How nsenter Works

`nsenter` allows entering the namespaces of another process:

```bash
nsenter --target <pid> --mount --uts --ipc --net --pid -- /bin/sh
```

**What happens:**
1. `--target <pid>` - Target container init process
2. `--mount` - Enter mount namespace (see container rootfs)
3. `--uts` - Enter UTS namespace (see container hostname)
4. `--ipc` - Enter IPC namespace (see container IPC)
5. `--net` - Enter network namespace (see container network)
6. `--pid` - Enter PID namespace (see container processes)
7. `-- /bin/sh` - Execute shell in container context

**Why /proc is required:**
- `nsenter` reads `/proc/<pid>/ns/*` to get namespace file descriptors
- Without `/proc` mounted, `nsenter` cannot work

### Interactive vs Command Mode

**Interactive (default):**
```bash
nk-runtime exec mycontainer
# Drops you into /bin/sh inside container
# Type commands interactively
# Type 'exit' to leave
```

**Command mode:**
```bash
nk-runtime exec --exec "ps -ef" mycontainer
# Executes 'ps -ef' in container
# Returns immediately with output
```

### Error Cases
- Container not found
- Container not running
- Container PID died (marked STOPPED)
- /proc not mounted
- nsenter not found
- Permission denied

---

## 5. DELETE Command

### Syntax
```bash
nk-runtime delete <container-id>
```

### Purpose
Stop running container (if needed) and cleanup all state and resources.

### Execution Flow

```mermaid
flowchart TD
    START[delete command] --> LOAD[Load container state]
    LOAD --> NOT_FOUND{Exists?}

    NOT_FOUND -->|no| SUCCESS[Return success]
    NOT_FOUND -->|yes| RUNNING{Running?}

    RUNNING -->|yes| SIGTERM[Send SIGTERM to PID]
    SIGTERM --> WAIT1[Wait 5 seconds]
    WAIT1 --> ALIVE{Still alive?}

    ALIVE -->|yes| SIGKILL[Send SIGKILL to PID]
    SIGKILL --> WAIT2[Wait 2 seconds]
    WAIT2 --> CLEANUP

    ALIVE -->|no| CLEANUP[Cleanup resources]
    RUNNING -->|no| CLEANUP

    CLEANUP --> CGROUP[Remove cgroup directory]
    CGROUP --> STATE_FILE[Remove state.json]
    STATE_FILE --> SUCCESS

    style SUCCESS fill:#e1f5e1
```

### Graceful vs Forced Termination

**Graceful termination (SIGTERM):**
- Allows process to cleanup
- Handle signals, close connections
- Wait 5 seconds for exit

**Forced termination (SIGKILL):**
- Immediate kernel termination
- No cleanup possible
- Used if SIGTERM fails

### Cleanup Steps

1. **Stop process** (if running)
   - Send SIGTERM
   - Wait up to 5 seconds
   - Send SIGKILL if needed

2. **Remove cgroup**
   ```bash
   rmdir /sys/fs/cgroup/nano-sandbox/<container-id>
   ```

3. **Remove state**
   ```bash
   rm /run/nano-sandbox/<container-id>/state.json
   rmdir /run/nano-sandbox/<container-id>
   ```

### Output Example
```
[08:42:15.123] [INFO] Deleting container 'test1'
[08:42:15.125] [INFO] Container running, sending SIGTERM
[08:42:15.130] [INFO] Container stopped
[08:42:15.131] [INFO] Removing cgroup
[08:42:15.132] [INFO] Removing state file
  Status: deleted
```

---

## 6. STATE Command

### Syntax
```bash
nk-runtime state <container-id>
```

### Purpose
Query and display container lifecycle state.

### Execution Flow

```mermaid
flowchart TD
    START[state command] --> LOAD[Load state.json]
    LOAD --> FOUND{Found?}

    FOUND -->|no| PRINT_UNKNOWN[Print: unknown]
    PRINT_UNKNOWN --> ERROR[Exit 1]

    FOUND -->|yes| VALIDATE{Valid JSON?}
    VALIDATE -->|no| ERROR
    VALIDATE -->|yes| PARSE[Parse state]

    PARSE --> PRINT[Print state string]
    PRINT --> SUCCESS[Exit 0]

    style SUCCESS fill:#e1f5e1
    style ERROR fill:#f5e1e1
```

### Output Examples

**Created container:**
```bash
$ nk-runtime state mycontainer
created
```

**Running container:**
```bash
$ nk-runtime state mycontainer
running
```

**Stopped container:**
```bash
$ nk-runtime state mycontainer
stopped
```

**Unknown container:**
```bash
$ nk-runtime state nonexistent
unknown
$ echo $?
1
```

### State Strings

| State | Meaning | Can Start? | Can Delete? |
|-------|---------|------------|-------------|
| `created` | Metadata saved, not running | ✅ Yes | ✅ Yes |
| `running` | Process active | ❌ No | ✅ Yes |
| `stopped` | Process exited | ❌ No | ✅ Yes |
| `unknown` | Not found | ❌ No | ❌ N/A |

---

## Lifecycle State Machine

Complete state transition diagram:

```mermaid
stateDiagram-v2
    [*] --> CREATED: create
    CREATED --> RUNNING: start
    CREATED --> DELETED: delete

    RUNNING --> RUNNING: exec
    RUNNING --> STOPPED: process exit
    RUNNING --> DELETED: delete

    STOPPED --> DELETED: delete

    DELETED --> [*]

    note right of CREATED
        Metadata saved
        No process running
    end note

    note right of RUNNING
        Process active
        exec allowed
    end note

    note right of STOPPED
        Process exited
        Cannot restart
    end note
```

## Error Handling Model

Errors are surfaced in three layers:

### 1. Return Codes
- `0` - Success
- `1` - Error (check logs for details)

### 2. Structured Logs
```
[timestamp] [LEVEL] [ROLE] message: details
                       ^^^^
                PARENT/CHILD/HOST
```

### 3. User-Facing Messages
```
Error: Failed to open state file: Permission denied
```

## Best Practices

### PID 1 Considerations

**Avoid shell as PID 1 for production:**
```json
"process": {
  "args": ["/bin/sh"]  // Bad: typing 'exit' stops container
}
```

**Use long-running process:**
```json
"process": {
  "args": ["/bin/busybox", "sh", "-c", "while :; do sleep 3600; done"]  // Good
}
```

**Then use exec for interaction:**
```bash
nk-runtime start mycontainer    # Starts in background
nk-runtime exec mycontainer     # Interactive shell
exit                            # Shell exits, container keeps running
nk-runtime delete mycontainer   # Cleanup when done
```

### State Management

**Always check state before operations:**
```bash
# Check if created before starting
nk-runtime state mycontainer | grep -q created && nk-runtime start mycontainer

# Check if running before exec
nk-runtime state mycontainer | grep -q running && nk-runtime exec mycontainer
```

**Cleanup after use:**
```bash
# Quick test with auto-cleanup
nk-runtime run --rm --bundle=/path/to/bundle test
```

## See Also

- [Architecture Overview](architecture-overview.md)
- [Kernel Mechanisms](kernel-mechanisms.md)
- [Container Ecosystem](container-ecosystem-lifecycle.md)
