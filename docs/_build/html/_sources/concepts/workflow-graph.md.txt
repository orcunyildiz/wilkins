# Workflow Graph

Wilkins models a workflow as a **directed graph** where nodes are tasks and edges are dataflow links. The graph is defined declaratively in a YAML configuration file.

<!-- TODO: Add workflow graph diagram (Figures 2-3 from the paper) -->

## Nodes

Each node represents a computational task — a producer, consumer, or a task that is both. A node is defined in YAML under the `tasks` list:

```yaml
tasks:
  - func: "./producer.py"
    nprocs: 3
    outports:
      - filename: "outfile.h5"
        dsets:
          - name: /group1/grid
            passthru: 0
            metadata: 1
```

Key properties of a node:

| Property | Description |
|---|---|
| `func` | Path to the task executable (`.py` script or `.so`/`.hx` shared object) |
| `nprocs` | Number of MPI processes allocated to this task |
| `nwriters` | (Optional) Subset of producer processes that participate in I/O |
| `taskCount` | (Optional) Number of ensemble instances of this task |
| `args` | (Optional) Command-line arguments passed to the task |
| `actions` | (Optional) External callback script and function name |
| `outports` | List of output ports (datasets this task produces) |
| `inports` | List of input ports (datasets this task consumes) |

## Links (edges)

Links are **automatically generated** by Wilkins from the port declarations. A link is created when an `outport` of one node matches an `inport` of another node on the same filename and dataset path.

For example, given:

```yaml
tasks:
  - func: "./producer.py"
    nprocs: 1
    outports:
      - filename: "outfile.h5"
        dsets:
          - name: /group1/grid
  - func: "./consumer.py"
    nprocs: 1
    inports:
      - filename: "outfile.h5"
        dsets:
          - name: /group1/grid
```

Wilkins automatically creates a link from the producer to the consumer on `outfile.h5/group1/grid`.

### Wildcard matching

Dataset names support `*` and `?` wildcards. Using `name: "*"` matches all datasets in the file:

```yaml
outports:
  - filename: "particles.h5"
    dsets:
      - name: "*"
        metadata: 1
```

This is convenient when a producer writes multiple datasets to the same file and the consumer reads all of them.

## Ports

Ports describe the HDF5 files and datasets that a task reads or writes. Each port has properties that control the data transport mode:

### Output ports (`outports`)

Declared on producer nodes. Each outport specifies:

- **`filename`** — The HDF5 filename (logical, not necessarily a real file on disk)
- **`dsets`** — List of datasets within the file, each with:
  - `name` — Dataset path (e.g., `/group1/grid`) or wildcard (`*`)
  - `metadata` — `1` for in-memory mode (default), `0` to disable
  - `passthru` — `1` for file mode, `0` to disable (default)
  - `zerocopy` — `1` for shallow/zero-copy transfer, `0` for deep copy (default)

### Input ports (`inports`)

Declared on consumer nodes. Same structure as outports, plus:

- **`io_freq`** — Flow control policy (see [Flow Control](flow-control.md))

## Automatic link generation

Wilkins uses a matching algorithm to generate links:

1. Each outport and inport is indexed by `filename/dataset_path`.
2. Outports produce **negative** indices; inports produce **positive** indices.
3. For each matching key, Wilkins pairs producers and consumers in a round-robin fashion.
4. If there are more consumers than producers (fan-out), consumers are distributed evenly across producers.
5. If there are more producers than consumers (fan-in), producers are distributed evenly across consumers.

This automatic matching means you never need to explicitly wire up connections — just declare what each task reads and writes, and Wilkins builds the graph.

## Execution groups

Each link belongs to an **execution group**, named `producer_func:consumer_func`. Execution groups determine which intercommunicators are created. Multiple links between the same producer-consumer pair share a single intercommunicator.

## Orphan ports

A port with no matching counterpart is called an **orphan port**:

- An **inport with no matching outport** means the task reads from a real file on disk. The port must use file mode (`passthru: 1, metadata: 0`), otherwise Wilkins raises an error.
- An **outport with no matching inport** means the task writes to a real file on disk. Same file-mode requirement applies.

This allows tasks to read input files or write output files that are not part of the in situ data flow.
