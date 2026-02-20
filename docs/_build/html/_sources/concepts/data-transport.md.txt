# Data Transport

Wilkins uses [LowFive](https://github.com/diatomic/LowFive) as its data transport layer. LowFive is an HDF5 VOL (Virtual Object Layer) plugin that intercepts standard HDF5 I/O calls and can either store data in memory for in situ transfer or pass them through to the native HDF5 file driver.

## The HDF5 VOL architecture

The HDF5 library supports a plugin architecture called the Virtual Object Layer (VOL). A VOL plugin intercepts all HDF5 API calls (file open/close, dataset create/read/write) and can redirect them to a custom backend.

LowFive implements this interface, acting as a transparent proxy between user code and the actual storage:

```
Task code (h5py / HDF5 C API)
        │
        ▼
   HDF5 library
        │
        ▼
  LowFive VOL plugin
        │
    ┌───┴───┐
    ▼       ▼
 Memory   Native HDF5
 (MPI)    (disk files)
```

Because LowFive operates at the VOL layer, **no source code changes** are needed in task codes that already use HDF5. The plugin is activated at runtime via environment variables (see [Environment Setup](../getting-started/environment.md)).

## Transport modes

Each dataset link in the workflow can operate in one of two modes, configured per-dataset in the YAML file.

### Memory mode (in situ)

```yaml
dsets:
  - name: /group1/grid
    metadata: 1
    passthru: 0
```

In memory mode (the default), LowFive:

1. Intercepts the producer's HDF5 write calls and stores the data in an in-memory metadata structure.
2. When the consumer issues HDF5 read calls for the same file and dataset, LowFive serves the data from the producer's memory via the MPI intercommunicator.

No files are created on disk, which is the key advantage of memory mode for in situ workflows. Data transfer happens entirely through MPI communication between producer and consumer processes, avoiding the overhead and latency of file system I/O.

### File mode (passthru)

```yaml
dsets:
  - name: /group1/grid
    metadata: 0
    passthru: 1
```

In file mode, LowFive passes HDF5 calls through to the native HDF5 driver:

1. The producer writes data to a real HDF5 file on disk.
2. The consumer reads from the same file.

File mode requires [callback actions](../user-guide/custom-actions.md) to synchronize producer and consumer (the producer must signal when a file is complete before the consumer reads it).

File mode is useful for:

- Debugging workflows (inspect intermediate HDF5 files)
- Checkpointing (persist data for restart)
- Workflows where consumer speed varies and buffering on disk is acceptable

### Choosing a mode

Both `metadata` and `passthru` cannot be disabled simultaneously — at least one must be `1`. If both are enabled on the same dataset, LowFive uses in-memory transfer and also writes to disk.

| `metadata` | `passthru` | Behavior |
|---|---|---|
| 1 | 0 | In-memory only (default) |
| 0 | 1 | File only (passthru) |
| 1 | 1 | In-memory + file |
| 0 | 0 | Error |

### Mode consistency

The transport mode must be consistent between the producer and consumer sides of a link:

- If the consumer requests memory mode but the producer does not enable it, Wilkins automatically falls back to passthru.
- If the consumer requests passthru but the producer does not enable it, Wilkins falls back to memory mode.

A warning is printed when such a fallback occurs.

## Zero-copy transfer

For large datasets where copying data between producer and consumer is expensive, LowFive supports **zero-copy** (shallow) transfer:

```yaml
outports:
  - filename: "outfile.h5"
    dsets:
      - name: /group1/grid
        metadata: 1
        zerocopy: 1
```

With zero-copy enabled, LowFive shares a pointer to the producer's data buffer instead of making a deep copy. This eliminates the memory copy overhead but requires that:

- The producer's data buffer remains valid until the consumer has finished reading.
- Producer and consumer share the same address space (they run on the same node).

Zero-copy is only available in memory mode and is set on the producer side (`outports`).

## MPI intercommunicators

Wilkins creates MPI intercommunicators to connect producer and consumer tasks. Each unique producer-consumer pair (execution group) gets one intercommunicator.

In **space-partitioned (SP) mode** — where producer and consumer occupy disjoint MPI ranks — a true MPI intercommunicator is created using `MPI_Intercomm_create`.

In **time-partitioned (TP) mode** — where producer and consumer share the same MPI ranks (e.g., the same code acts as both) — a duplicated intracommunicator is used instead.

The intercommunicators are passed to LowFive's `create_DistMetadataVOL()` function, which uses them to route data between tasks.

## See also

- [YAML Reference](../user-guide/yaml-reference.md) — full list of transport-related fields (`metadata`, `passthru`, `zerocopy`)
- [Custom Actions](../user-guide/custom-actions.md) — callback functions required for file mode synchronization
- [Environment Setup](../getting-started/environment.md) — how to configure the LowFive VOL plugin at runtime
