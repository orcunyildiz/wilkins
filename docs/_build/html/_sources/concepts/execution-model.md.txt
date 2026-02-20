# Execution Model

Wilkins supports running heterogeneous tasks — mixing Python and C++ codes — within a single MPI job. This page describes how tasks are launched and how they interact with the Wilkins runtime.

## Master mode (default)

In master mode, all tasks run within a single `mpirun` invocation, coordinated by the `wilkins-master` driver:

```bash
mpirun -n <total_procs> wilkins-master config.yaml
```

The driver:

1. Parses the YAML configuration and assigns each task a contiguous range of MPI ranks from `MPI_COMM_WORLD`.
2. Creates MPI sub-communicators (one per task) and intercommunicators (one per producer-consumer execution group).
3. Launches each task using [Henson](https://github.com/henson-insitu/henson) coroutines, which provide cooperative multitasking within a single address space.
4. Manages the **consumer loop**: after the initial task execution, consumers repeatedly poll for new data from producers until the producer signals completion.

### Process allocation

Processes are allocated sequentially in the order tasks appear in the YAML file. For example:

```yaml
tasks:
  - func: "./producer.py"
    nprocs: 3
  - func: "./consumer1.py"
    nprocs: 2
  - func: "./consumer2.py"
    nprocs: 1
```

| Task | Ranks |
|---|---|
| producer | 0, 1, 2 |
| consumer1 | 3, 4 |
| consumer2 | 5 |

Total processes required: `mpirun -n 6 wilkins-master config.yaml`

### Subset writers

For large simulations where not all producer processes generate output data, the `nwriters` field limits which processes participate in LowFive I/O:

```yaml
tasks:
  - func: "./producer.hx"
    nprocs: 4
    nwriters: 1
```

Only the first `nwriters` processes of the producer join the LowFive communicator. The remaining processes skip I/O operations. This reduces communication overhead when only a subset of simulation processes produce data.

## Python tasks

Python task scripts must define a `main()` function:

```python
def main(task_args=None):
    # task_args is an optional list of command-line arguments
    ...
```

The driver imports the script and calls `main()` directly. No compilation is needed. See [Writing Python Tasks](../user-guide/python-tasks.md) for details.

## C++ tasks (Henson)

C++ tasks are compiled as shared objects (`.so` or `.hx` files) and linked with [Henson](https://github.com/henson-insitu/henson). They run as coroutines managed by the Henson runtime, which Wilkins integrates via the `pyhenson` Python bindings.

C++ tasks use the HDF5 C API directly (not h5py). LowFive intercepts these calls just as it does for Python tasks. See [Writing C++ Tasks](../user-guide/cpp-tasks.md) for compilation details.

## Consumer loop

In a typical in situ workflow, the producer runs iteratively (e.g., a simulation time-stepping loop), writing data at each step. The consumer processes this data as it becomes available.

Wilkins implements this pattern through a **consumer loop** in the master driver:

1. The producer writes data via HDF5 calls. LowFive stores it in memory (or on disk in file mode).
2. LowFive's `serve_all()` makes the data available to consumers.
3. The consumer calls `vol.get_filenames()` to check for new data.
4. If data is available, the consumer's `main()` is called again.
5. When the producer finishes, it calls `vol.producer_done()`, which signals consumers to stop.

This loop is managed entirely by the Wilkins driver — task codes do not need to implement any loop logic themselves. Wilkins handles iteration and termination detection internally, so tasks can focus on their computational work.

## Stateful vs. stateless consumers

### Stateful consumers

A stateful consumer is called repeatedly for each new dataset the producer generates. It can maintain state across iterations because `main()` is called multiple times within the same process. This is useful for consumers that accumulate results (e.g., computing running averages, building visualizations incrementally).

The consumer loop in `wilkins-master` handles this automatically: after the initial `main()` call, the driver keeps polling for new data and re-invoking `main()`.

### Stateless consumers

A stateless consumer processes exactly one dataset and does not need to persist state. This is the simpler case: `main()` is called once per data batch. If the producer writes multiple time steps, the consumer is invoked once per step.

Both patterns use the same consumer loop mechanism — the distinction is in the task code's design, not in the Wilkins configuration.

## Passthru mode (file-based)

When all ports on a link use passthru mode (`passthru: 1, metadata: 0`), data flows through actual HDF5 files on disk rather than in-memory metadata. This requires **callback actions** to synchronize the producer and consumer:

- The producer registers a `prod_callback` that signals downstream consumers after writing a file.
- The consumer registers a `con_callback` that blocks until the upstream producer signals data is ready.

See [Custom Actions](../user-guide/custom-actions.md) for how to configure callback actions in YAML.

## See also

- [Writing Python Tasks](../user-guide/python-tasks.md) — how to structure Python task scripts for use with Wilkins
- [Writing C++ Tasks](../user-guide/cpp-tasks.md) — compilation and structure of C++ shared object tasks
- [Running Workflows](../user-guide/running.md) — how to launch workflows and calculate process counts
