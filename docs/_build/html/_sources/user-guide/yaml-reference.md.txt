# YAML Reference

The YAML configuration file is the central piece of any Wilkins workflow. It describes the tasks, their data dependencies, and how data should be transported between them. Wilkins reads this file at startup and uses it to build the entire workflow graph, allocate MPI processes, and configure LowFive's data transport layer. This page documents every field available in the configuration file.

## Top-level structure

A Wilkins YAML file has a single top-level key:

```yaml
tasks:
  - ...   # first task
  - ...   # second task
```

## Task fields

Each entry under `tasks` defines a workflow node:

```yaml
tasks:
  - func: "./producer.py"
    nprocs: 3
    nwriters: 1
    taskCount: 2
    args: ["5", "--output", "data.h5"]
    actions: ["passthru-actions", "prod_callback"]
    outports:
      - ...
    inports:
      - ...
```

| Field | Type | Required | Default | Description |
|---|---|---|---|---|
| `func` | string | yes | — | Path to the task executable. For Python tasks, a `.py` file with a `main()` function. For C++ tasks, a `.so` or `.hx` shared object. |
| `nprocs` | int | yes | — | Number of MPI processes allocated to this task. |
| `nwriters` | int | no | `-1` (all) | Number of producer processes that participate in LowFive I/O. Only the first `nwriters` ranks join the I/O communicator. Set to `-1` to include all processes. |
| `taskCount` | int | no | `1` | Number of ensemble instances of this task. Each instance gets its own `nprocs` processes. See [Topologies](topologies.md) for ensemble examples. |
| `args` | list of strings | no | `[]` | Command-line arguments passed to the task. The special placeholder `{filename}` is replaced at runtime with the actual filename provided by LowFive. |
| `actions` | list of strings | no | `[]` | External callback action, specified as `["module_name", "function_name"]`. See [Custom Actions](custom-actions.md). |
| `outports` | list | no | `[]` | Output ports — datasets this task produces. |
| `inports` | list | no | `[]` | Input ports — datasets this task consumes. |

## Output port fields (`outports`)

```yaml
outports:
  - filename: "outfile.h5"
    dsets:
      - name: /group1/grid
        metadata: 1
        passthru: 0
        zerocopy: 0
```

| Field | Type | Required | Default | Description |
|---|---|---|---|---|
| `filename` | string | yes | — | Logical HDF5 filename. Does not need to correspond to a real file on disk (in memory mode). |
| `range` | list of ints | no | `[1..100]` | Instance indices for ensemble filename generation. |
| `dsets` | list | yes | — | Datasets within this file. |

### Dataset fields (outport)

| Field | Type | Required | Default | Description |
|---|---|---|---|---|
| `name` | string | yes | — | Dataset path (e.g., `/group1/grid`). Supports wildcards: `*` matches all datasets. |
| `metadata` | int (0 or 1) | no | `1` | Enable in-memory mode. Data is stored in LowFive's metadata and transferred via MPI. |
| `passthru` | int (0 or 1) | no | `0` | Enable file mode. Data is written to a real HDF5 file on disk. |
| `zerocopy` | int (0 or 1) | no | `0` | Enable zero-copy (shallow) transfer. Shares the producer's data buffer instead of making a deep copy. Only applies in memory mode. |

At least one of `metadata` or `passthru` must be `1`.

## Input port fields (`inports`)

```yaml
inports:
  - filename: "outfile.h5"
    io_freq: 2
    dsets:
      - name: /group1/grid
        metadata: 1
        passthru: 0
```

| Field | Type | Required | Default | Description |
|---|---|---|---|---|
| `filename` | string | yes | — | Logical HDF5 filename. Must match an `outport` filename in another task for automatic link creation. |
| `io_freq` | int or string | no | `1` | Flow control policy. `1` = receive all data, `N` = receive every Nth dataset, `"latest"` = receive only the most recent. See [Flow Control](../concepts/flow-control.md). |
| `range` | list of ints | no | `[1..100]` | Instance indices for ensemble filename generation. |
| `dsets` | list | yes | — | Datasets within this file. |

### Dataset fields (inport)

| Field | Type | Required | Default | Description |
|---|---|---|---|---|
| `name` | string | yes | — | Dataset path. Must match an outport dataset name (wildcards supported). |
| `metadata` | int (0 or 1) | no | `1` | Enable in-memory mode. |
| `passthru` | int (0 or 1) | no | `0` | Enable file mode. |

At least one of `metadata` or `passthru` must be `1`.

## Complete example

A 3-task workflow with a producer writing to two consumers, using memory mode:

```yaml
tasks:
  - func: "./producer.py"
    nprocs: 1
    outports:
      - filename: "outfile.h5"
        dsets:
          - name: /particles
            metadata: 1
      - filename: "outfile.h5"
        dsets:
          - name: /grid
            metadata: 1
  - func: "./consumer1.py"
    nprocs: 1
    inports:
      - filename: "outfile.h5"
        dsets:
          - name: /particles
            metadata: 1
  - func: "./consumer2.py"
    nprocs: 1
    inports:
      - filename: "outfile.h5"
        dsets:
          - name: /grid
            metadata: 1
```

Run with:

```bash
mpirun -n 3 wilkins-master config.yaml
```

## Link generation rules

Links between tasks are generated automatically by matching outport and inport filenames and dataset paths:

1. Each `filename/dataset` pair is collected across all tasks.
2. Producers contribute **negative** indices; consumers contribute **positive** indices.
3. If there are fewer producers than consumers, consumers are distributed round-robin across producers (fan-out).
4. If there are fewer consumers than producers, producers are distributed round-robin across consumers (fan-in).
5. A port with no matching counterpart is an **orphan port** — the task reads from or writes to a real file. Orphan ports must use file mode (`passthru: 1, metadata: 0`).
