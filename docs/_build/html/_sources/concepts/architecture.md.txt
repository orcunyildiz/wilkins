# Architecture

Wilkins is a middleware system that sits between user task codes and the MPI runtime, orchestrating in situ workflows where multiple tasks run concurrently and exchange data without writing to the file system.

<!-- TODO: Add system architecture diagram (Figure 1 from the paper) -->

## Components

A Wilkins deployment consists of three main components:

### 1. Workflow driver (`wilkins-master`)

The workflow driver is the entry point for every Wilkins run. It:

- Parses the YAML workflow configuration
- Builds the workflow graph (nodes and links)
- Allocates MPI processes to tasks based on the `nprocs` field in the YAML
- Creates MPI sub-communicators for each task and intercommunicators between coupled tasks
- Configures LowFive VOL properties (memory mode, passthru mode, zero-copy)
- Launches task codes via [Henson](https://github.com/henson-insitu/henson) coroutines (C++ tasks) or direct Python import (Python tasks)
- Manages the consumer loop (repeated data fetching until the producer signals completion)

The driver runs under `mpirun` and coordinates all processes in `MPI_COMM_WORLD`:

```bash
mpirun -n <total_procs> wilkins-master config.yaml
```

### 2. LowFive — data transport layer

[LowFive](https://github.com/diatomic/LowFive) is an HDF5 VOL (Virtual Object Layer) plugin that intercepts HDF5 I/O calls and redirects them. It provides two transport modes:

- **Memory mode** (`metadata: 1, passthru: 0`) — Data is held in memory and transferred between tasks via MPI. No files are written to disk. This is the default and preferred mode for in situ workflows.
- **File mode / passthru** (`metadata: 0, passthru: 1`) — Data is written to and read from actual HDF5 files on disk. Useful when tasks must operate on persistent files or when debugging.

LowFive is transparent to user code: tasks use standard HDF5 calls (via h5py in Python or the HDF5 C API in C++) without modification.

### 3. User task codes

Tasks are the computational units of a workflow. They can be:

- **Python scripts** — A `.py` file with a `main()` function. No compilation needed.
- **C++ shared objects** — Compiled as `.so` or `.hx` shared libraries linked with [Henson](https://github.com/henson-insitu/henson). Required for legacy C/C++ simulation codes.

Tasks read and write data using standard HDF5 calls. LowFive intercepts these calls at runtime and routes data according to the workflow configuration.

## How it fits together

```
 User tasks                  Wilkins                    Infrastructure
┌──────────┐              ┌──────────────┐             ┌──────────┐
│producer.py│──h5py───────│  LowFive VOL │─────MPI─────│  MPI     │
└──────────┘              │  (intercept) │             │ runtime  │
                          └──────┬───────┘             └──────────┘
┌──────────┐                     │
│consumer.py│──h5py──────────────┘
└──────────┘

          ┌──────────────────────────────┐
          │ wilkins-master (orchestrator) │
          │  - YAML parsing              │
          │  - Process allocation         │
          │  - Communicator setup         │
          │  - Task launching             │
          │  - Consumer loop management   │
          └──────────────────────────────┘
```

1. `wilkins-master` reads the YAML configuration and partitions `MPI_COMM_WORLD` into sub-communicators.
2. Each task is launched in its own MPI sub-communicator.
3. Intercommunicators are created between coupled producer-consumer pairs.
4. LowFive intercepts HDF5 calls and routes data through these intercommunicators (memory mode) or to/from disk (file mode).
5. The consumer loop in `wilkins-master` repeatedly calls `vol.get_filenames()` until the producer signals it is done via `vol.producer_done()`.

## Design principles

**Data-centric coupling.** Tasks are coupled through their data dependencies (HDF5 files and datasets), not through explicit message passing. This means existing HDF5-based codes can be coupled with minimal or no modification.

**Separation of concerns.** The workflow topology, data transport mode, and process allocation are all specified externally in the YAML configuration. Task codes do not need to know about the workflow structure.

**Flexible transport.** The same workflow can run in memory mode (for production in situ) or file mode (for debugging or checkpointing) by changing a single flag in the YAML.
