# Quickstart

This guide walks through running a simple producer-consumer workflow end-to-end using pure Python tasks. It takes about 5 minutes.

## Overview

We will run a workflow with two tasks:

- **Producer** — creates an HDF5 dataset in memory
- **Consumer** — reads the dataset from the producer (in situ, no files on disk)

Wilkins and LowFive handle the data transport transparently: both tasks use standard `h5py` calls as if they were writing to and reading from a regular HDF5 file.

## Prerequisites

Make sure you have completed [Installation](installation.md) and [Environment Setup](environment.md).

## Step 1 — Write the producer

Create a file called `producer.py`:

```python
import h5py
import numpy as np

def main():
    f = h5py.File('particles.h5', 'w')
    f.create_dataset("data", data=np.ones((4, 3, 2), 'f'))
    f.close()

if __name__ == "__main__":
    main()
```

The producer creates a 4x3x2 NumPy array and writes it to `particles.h5` via h5py. When run under Wilkins with LowFive, this write goes to in-memory metadata rather than to a file on disk.

## Step 2 — Write the consumer

Create a file called `consumer.py`:

```python
import h5py

def main(task_args=None):
    f = h5py.File("particles.h5", "r")
    data = f["data"][:]
    print(data)
    f.close()

if __name__ == "__main__":
    main()
```

The consumer reads the same file and dataset that the producer wrote. LowFive intercepts the HDF5 read and serves the data from the producer's memory.

Note the `task_args` parameter: Wilkins passes parsed command-line arguments to `main()` if the function accepts them. This is optional.

## Step 3 — Write the YAML configuration

Create a file called `wilkins_prod_con.yaml`:

```yaml
tasks:
  - func: "./producer.py"
    nprocs: 1
    outports:
      - filename: "particles.h5"
        dsets:
          - name: "*"
            passthru: 0
            metadata: 1
  - func: "./consumer.py"
    nprocs: 1
    inports:
      - filename: "particles.h5"
        dsets:
          - name: "*"
            passthru: 0
            metadata: 1
```

Key fields:

- **`func`** — path to the Python task script (must define a `main()` function)
- **`nprocs`** — number of MPI processes allocated to this task
- **`outports` / `inports`** — declare which HDF5 files and datasets the task writes or reads
- **`passthru: 0, metadata: 1`** — in-memory mode (data stays in memory, no file I/O)
- **`name: "*"`** — wildcard matching all datasets in the file

The YAML configuration defines the **data dependencies** between tasks. Wilkins automatically creates the workflow graph and MPI communicators from this description.

## Step 4 — Run the workflow

```bash
mpirun -n 2 wilkins-master wilkins_prod_con.yaml
```

:::{note}
The total number of MPI processes (`-n 2`) must equal the sum of `nprocs` across all tasks (1 producer + 1 consumer = 2). If these numbers do not match, the workflow will not run correctly.
:::

You should see the consumer print the 4x3x2 array of ones:

```
[[[1. 1.]
  [1. 1.]
  [1. 1.]]

 [[1. 1.]
  [1. 1.]
  [1. 1.]]

 [[1. 1.]
  [1. 1.]
  [1. 1.]]

 [[1. 1.]
  [1. 1.]
  [1. 1.]]]
```

## What just happened?

1. `wilkins-master` parsed the YAML configuration and built a workflow graph with two nodes (producer, consumer) connected by a dataflow link on `particles.h5`.
2. It allocated MPI rank 0 to the producer and rank 1 to the consumer.
3. LowFive intercepted the producer's `h5py.File('particles.h5', 'w')` call and stored the dataset in memory.
4. When the consumer called `h5py.File('particles.h5', 'r')`, LowFive served the data from the producer's memory via MPI, with no file on disk.

## Next steps

- Learn about [environment variables](environment.md) required for LowFive
- Understand the [YAML configuration format](../user-guide/yaml-reference.md) in detail
- Explore [workflow topologies](../user-guide/topologies.md) (fan-out, fan-in, cycles)
- Read about the [architecture](../concepts/architecture.md) behind Wilkins
