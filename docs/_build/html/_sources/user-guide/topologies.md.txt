# Workflow Topologies

Wilkins supports any directed-graph topology of tasks. This page serves as a gallery of the most common workflow patterns, with ready-to-use YAML configurations for each. These examples can be used as starting points and adapted to your own applications.

## Pipeline

A linear chain of tasks where each task consumes data from the previous one and produces data for the next.

```
producer → consumer1 → consumer2
```

```yaml
tasks:
  - func: "./producer.py"
    nprocs: 2
    outports:
      - filename: "step1.h5"
        dsets:
          - name: "*"
            metadata: 1
  - func: "./stage1.py"
    nprocs: 2
    inports:
      - filename: "step1.h5"
        dsets:
          - name: "*"
            metadata: 1
    outports:
      - filename: "step2.h5"
        dsets:
          - name: "*"
            metadata: 1
  - func: "./stage2.py"
    nprocs: 1
    inports:
      - filename: "step2.h5"
        dsets:
          - name: "*"
            metadata: 1
```

Run with: `mpirun -n 5 wilkins-master pipeline.yaml`

## Fan-out (one producer, multiple consumers)

One producer distributes data to multiple consumers. Each consumer reads different datasets from the same file.

```
             ┌─→ consumer1 (reads /particles)
producer ────┤
             └─→ consumer2 (reads /grid)
```

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

Run with: `mpirun -n 3 wilkins-master fanout.yaml`

Consumers can also read the same dataset — Wilkins creates separate links for each consumer.

## Fan-in (multiple producers, one consumer)

Multiple producers send data to a single consumer. Each producer writes to a different file.

```
producer1 (writes outfile1.h5) ──┐
                                 ├─→ consumer
producer2 (writes outfile2.h5) ──┘
```

```yaml
tasks:
  - func: "./producer1.py"
    nprocs: 1
    outports:
      - filename: "outfile1.h5"
        dsets:
          - name: "*"
            metadata: 1
  - func: "./producer2.py"
    nprocs: 1
    outports:
      - filename: "outfile2.h5"
        dsets:
          - name: "*"
            metadata: 1
  - func: "./consumer.py"
    nprocs: 1
    inports:
      - filename: "outfile1.h5"
        dsets:
          - name: "*"
            metadata: 1
      - filename: "outfile2.h5"
        dsets:
          - name: "*"
            metadata: 1
```

Run with: `mpirun -n 3 wilkins-master fanin.yaml`

## Ensembles

Ensembles run multiple instances of the same task. Use the `taskCount` field to specify the number of instances. Each instance gets its own `nprocs` processes.

### NxN ensemble (3 producers, 3 consumers)

```yaml
tasks:
  - taskCount: 3
    func: prod-ensemble
    nprocs: 2
    args: ["2"]
    outports:
      - filename: "*.h5"
        dsets:
          - name: /group1/grid
            metadata: 1
          - name: /group1/particles
            metadata: 1
  - taskCount: 3
    func: con-ensemble
    nprocs: 2
    args: ["2", "1", "{filename}"]
    inports:
      - filename: "*.h5"
        dsets:
          - name: /group1/grid
            metadata: 1
          - name: /group1/particles
            metadata: 1
```

Run with: `mpirun -n 12 wilkins-master ensemble_NxN.yaml` (3x2 + 3x2 = 12 processes)

Wilkins automatically creates one-to-one links between matching producer and consumer instances.

### Ensemble fan-out (2 producers, 4 consumers)

```yaml
tasks:
  - taskCount: 2
    func: prod-ensemble
    nprocs: 2
    outports:
      - filename: "*.h5"
        dsets:
          - name: /group1/grid
            metadata: 1
  - taskCount: 4
    func: con-ensemble
    nprocs: 2
    inports:
      - filename: "*.h5"
        dsets:
          - name: /group1/grid
            metadata: 1
```

Consumers are distributed round-robin across producers: producer 0 serves consumers 0 and 1, producer 1 serves consumers 2 and 3.

### Ensemble fan-in (4 producers, 2 consumers)

```yaml
tasks:
  - taskCount: 4
    func: prod-ensemble
    nprocs: 2
    outports:
      - filename: "*.h5"
        dsets:
          - name: /group1/grid
            metadata: 1
  - taskCount: 2
    func: con-ensemble
    nprocs: 2
    inports:
      - filename: "*.h5"
        dsets:
          - name: /group1/grid
            metadata: 1
```

Producers are distributed round-robin across consumers: consumer 0 receives from producers 0 and 1, consumer 1 receives from producers 2 and 3.

### Ensemble filename convention

When `taskCount > 1`, Wilkins automatically appends an instance suffix to filenames:

- `outfile.h5` becomes `outfile-inst1.h5`, `outfile-inst2.h5`, etc.

The `range` field can customize the instance indices:

```yaml
outports:
  - filename: "output.h5"
    range: [10, 20, 30]
    dsets:
      - name: "*"
        metadata: 1
```

This produces `output-inst10.h5`, `output-inst20.h5`, `output-inst30.h5`.

## Cycles

A cycle topology has tasks that form a loop: the output of the last task feeds back as input to the first.

```
node0 → node1 → node2 → node0
```

```yaml
tasks:
  - func: "./node0.hx"
    nprocs: 1
    args: ["5", "-s"]
    inports:
      - filename: "outfile2.h5"
        dsets:
          - name: /group1/particles
            metadata: 1
    outports:
      - filename: "outfile0.h5"
        dsets:
          - name: /group1/grid
            metadata: 1
          - name: /group1/particles
            metadata: 1
  - func: "./node1.hx"
    nprocs: 1
    args: ["5"]
    inports:
      - filename: "outfile0.h5"
        dsets:
          - name: /group1/grid
            metadata: 1
          - name: /group1/particles
            metadata: 1
    outports:
      - filename: "outfile1.h5"
        dsets:
          - name: /group1/particles
            metadata: 1
  - func: "./node2.hx"
    nprocs: 1
    args: ["5"]
    inports:
      - filename: "outfile1.h5"
        dsets:
          - name: /group1/particles
            metadata: 1
    outports:
      - filename: "outfile2.h5"
        dsets:
          - name: /group1/particles
            metadata: 1
```

Run with: `mpirun -n 3 wilkins-master cycle.yaml`

Key points for cycles:

- Each node has both `inports` and `outports`.
- Use **distinct filenames** for each link in the cycle (e.g., `outfile0.h5`, `outfile1.h5`, `outfile2.h5`).
- Cycles work in both memory and file (passthru) mode. File mode requires [callback actions](custom-actions.md) on each node.
- Wilkins resolves producer-done ordering automatically to avoid deadlocks.

## Process count formula

The total number of MPI processes required is the sum of `nprocs * taskCount` across all tasks:

```
total = sum(task.nprocs * task.taskCount for task in tasks)
```

For example, a workflow with:
- Producer: `nprocs: 4, taskCount: 2` → 8 processes
- Consumer: `nprocs: 2, taskCount: 3` → 6 processes

Requires `mpirun -n 14 wilkins-master config.yaml`.
