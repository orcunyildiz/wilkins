# Writing Python Tasks

Python tasks are the simplest way to write Wilkins workflow components. No compilation is needed — just write a Python script with a `main()` function and reference it in the YAML configuration.

## Basic structure

A Python task must define a `main()` function:

```python
def main(task_args=None):
    """
    :param task_args: optional list of command-line arguments from the YAML config
    """
    # Your task logic here
    ...
```

The `task_args` parameter is optional. If defined, Wilkins passes the `args` list from the YAML configuration. If your `main()` does not accept any arguments, Wilkins calls it without arguments.

## Producer example

A producer writes data using standard h5py calls:

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

When run under Wilkins with LowFive in memory mode, the `h5py.File('particles.h5', 'w')` call is intercepted by the LowFive VOL plugin. Data is stored in memory rather than written to disk.

## Consumer example

A consumer reads data using standard h5py calls:

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

LowFive intercepts the read and serves data from the producer's memory via MPI.

## Using task arguments

Arguments defined in the YAML `args` field are passed as a list of strings:

```yaml
tasks:
  - func: "./analysis.py"
    nprocs: 2
    args: ["--threshold", "0.5", "--output", "results.h5"]
    inports:
      - ...
```

```python
import argparse

def main(task_args=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--threshold", type=float, default=1.0)
    parser.add_argument("--output", type=str, default="out.h5")
    args = parser.parse_args(task_args or [])
    
    # Use args.threshold, args.output, etc.
    ...
```

### Dynamic filenames

The special placeholder `{filename}` in the args list is replaced at runtime with the actual filename provided by LowFive:

```yaml
args: ["5", "{filename}"]
```

This is useful for stateful consumers that need to know which file to read.

## Using MPI within tasks

Tasks run in their own MPI sub-communicator. You can use MPI within a task, but you should **not** use `MPI.COMM_WORLD` — it spans the entire workflow, not just your task.

Instead, use Wilkins to get the local communicator:

```python
from wilkins.wilkins import Wilkins, get_local_comm
from mpi4py import MPI

def main():
    wilkins = Wilkins(MPI.COMM_WORLD, "config.yaml")
    comm = get_local_comm(wilkins)
    
    rank = comm.Get_rank()
    size = comm.Get_size()
    print(f"Task rank {rank} of {size}")
```

Note: in master mode (the default, using `wilkins-master`), the driver handles communicator setup automatically. The above pattern is useful when your task needs to perform collective MPI operations internally (e.g., parallel I/O or domain decomposition).

## YAML configuration

Reference the Python script path in the `func` field:

```yaml
tasks:
  - func: "./producer.py"
    nprocs: 1
    outports:
      - filename: "particles.h5"
        dsets:
          - name: "*"
            metadata: 1
```

The path is resolved relative to the working directory where `wilkins-master` is run. Use `./` prefix for scripts in the current directory.

## How tasks are launched

In master mode, `wilkins-master` imports the Python script as a module and calls its `main()` function directly:

1. The script path is resolved and the directory is added to `sys.path`.
2. The module is imported using `importlib.import_module()`.
3. `main(task_args)` is called. If that raises a `TypeError` (because `main()` doesn't accept arguments), `main()` is called without arguments.
4. For consumers in the consumer loop, `main()` is called again each time new data is available.

Because tasks are imported as modules, any module-level code (outside `main()`) runs only once. The `if __name__ == "__main__"` guard lets you also run the script standalone for testing.

## Tips

- **Keep `main()` idempotent when possible.** For stateful consumers, `main()` is called multiple times. It is a good practice to ensure it can handle being called repeatedly without unexpected side effects.
- **Close HDF5 files.** Always call `f.close()` after reading or writing. LowFive needs the close call to trigger data transfer.
- **Use `if __name__ == "__main__"` guards.** This lets you test your script outside of Wilkins.
- **Avoid global state.** Module-level variables persist across `main()` calls in the consumer loop. Use local variables inside `main()` unless you intentionally need state across iterations.
