# Flow Control

When a producer generates data faster than a consumer can process it, flow control determines which data the consumer receives. Wilkins supports per-link flow control policies configured via the `io_freq` field on consumer inports.

## The problem

In a typical in situ workflow, the producer (e.g., a simulation) runs a time-stepping loop and outputs data at each step. If the consumer (e.g., an analysis routine) takes longer to process each step than the producer takes to generate it, data accumulates. Without flow control, the consumer would fall further and further behind.

Wilkins addresses this with three flow control strategies.

## Strategies

### All (`io_freq: 1`)

```yaml
inports:
  - filename: "*.h5"
    io_freq: 1
    dsets:
      - name: /group1/grid
        metadata: 1
```

The consumer receives **every** dataset the producer generates. This is the default. If the consumer is slower than the producer, data is buffered until the consumer catches up.

Use this when no data loss is acceptable (e.g., the consumer computes a global aggregate that requires every time step).

### Some (`io_freq: N` where N > 1)

```yaml
inports:
  - filename: "*.h5"
    io_freq: 2
    dsets:
      - name: /group1/grid
        metadata: 1
```

The consumer receives every Nth dataset. In this example with `io_freq: 2`, the consumer processes steps 1, 3, 5, 7, ... and skips steps 2, 4, 6, 8, ...

Use this for periodic analysis or visualization where processing every step is unnecessary.

### Latest (`io_freq: "latest"`)

```yaml
inports:
  - filename: "*.h5"
    io_freq: "latest"
    dsets:
      - name: /group1/grid
        metadata: 1
```

The consumer always receives the **most recent** dataset, skipping any intermediate steps that were produced while the consumer was busy. When the consumer finishes processing one step and asks for the next, it gets whatever the producer most recently generated.

Use this for real-time monitoring or visualization where only the current state matters.

Internally, the latest policy uses `MPI_Iprobe` on the intercommunicator to detect whether new data is available. If no new data has arrived since the last check, the consumer skips the serve cycle for that link.

## Per-link configuration

Flow control is configured **per inport**, not globally. Different consumer inports in the same workflow can use different policies:

```yaml
tasks:
  - func: "./simulation.py"
    nprocs: 4
    outports:
      - filename: "output.h5"
        dsets:
          - name: /temperature
            metadata: 1
          - name: /velocity
            metadata: 1
  - func: "./analysis.py"
    nprocs: 2
    inports:
      - filename: "output.h5"
        io_freq: 1
        dsets:
          - name: /temperature
            metadata: 1
  - func: "./visualization.py"
    nprocs: 1
    inports:
      - filename: "output.h5"
        io_freq: "latest"
        dsets:
          - name: /velocity
            metadata: 1
```

In this example, the analysis task receives every temperature dataset, while the visualization task only receives the latest velocity dataset.

## Implementation

Flow control is implemented in the `FlowControl` class in the master driver (`wilkins/master.py`). The driver registers a callback with LowFive's `vol.set_serve_indices()` that decides which intercommunicator indices to serve on each iteration:

- For `io_freq: N` — the callback tracks a counter and skips links when `counter % N != 0`.
- For `io_freq: "latest"` — the callback uses `MPI_Iprobe` with `tag=2` on the intercommunicator and only serves if the consumer has signaled readiness.
- For `io_freq: 1` — the link is always served (no filtering).

## See also

- [YAML Reference](../user-guide/yaml-reference.md) — full specification of the `io_freq` field on inports
- [Data Transport](data-transport.md) — how memory mode and file mode interact with flow control
