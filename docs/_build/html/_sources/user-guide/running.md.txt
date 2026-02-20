# Running Workflows

This page covers how to launch Wilkins workflows, command-line options, and practical tips.

## Basic invocation

```bash
mpirun -n <total_procs> wilkins-master config.yaml [options]
```

Or equivalently, using the Python module directly:

```bash
mpirun -n <total_procs> python -m wilkins.master config.yaml [options]
```

Both forms are identical — `wilkins-master` is a console script that calls `wilkins.master:main`.

## Command-line options

```
wilkins-master config.yaml [-p 0|1] [-v 0|1|2]
```

| Option | Long form | Values | Default | Description |
|---|---|---|---|---|
| `-p` | `--passthruSupport` | `0`, `1` | `0` | Passthru support level. `0`: none (normal multi-iteration). `1`: single-iteration passthru mode — the consumer calls `wait()` before execution and `commit()` after, enabling one-shot file exchange. |
| `-v` | `--verbosity` | `0`, `1`, `2` | `0` | Logging level. `0`: none. `1`: info-level LowFive logs. `2`: debug-level LowFive logs. |

The YAML config file must be the **first** positional argument, before any options.

## Calculating the process count

The total number of MPI processes must equal the sum of all `nprocs * taskCount` values in the YAML:

```yaml
tasks:
  - func: "./producer.py"
    nprocs: 4
  - func: "./consumer.py"
    nprocs: 2
```

```bash
mpirun -n 6 wilkins-master config.yaml   # 4 + 2 = 6
```

For ensembles:

```yaml
tasks:
  - func: prod-ensemble
    nprocs: 2
    taskCount: 3
  - func: con-ensemble
    nprocs: 2
    taskCount: 3
```

```bash
mpirun -n 12 wilkins-master config.yaml   # (2*3) + (2*3) = 12
```

If the total process count does not match, MPI ranks will be misallocated and the workflow will likely hang or produce unexpected results.

## Environment setup

Before running, ensure the LowFive environment is configured:

```bash
export HDF5_VOL_CONNECTOR="lowfive under_vol=0;under_info={};"
export HDF5_PLUGIN_PATH=/path/to/lowfive/build/src
```

Or with Spack:

```bash
spack load wilkins
```

See [Environment Setup](../getting-started/environment.md) for details.

## Working directory

Task script paths in the YAML `func` field are resolved relative to the current working directory. Make sure you run `wilkins-master` from the correct directory, or use absolute paths:

```bash
# From the examples directory
cd examples/python
mpirun -n 2 wilkins-master wilkins_prod_con.yaml

# Or with absolute paths in the YAML
# func: "/path/to/examples/python/producer.py"
```

For file-mode (passthru) workflows, output HDF5 files are also written to the current working directory.

## Example: running the Python producer-consumer

```bash
cd /path/to/wilkins/examples/python
mpirun -n 2 wilkins-master wilkins_prod_con.yaml
```

This runs the basic producer-consumer example with 1 producer process and 1 consumer process in memory mode.

## Example: running with passthru and verbosity

```bash
mpirun -n 2 wilkins-master config.yaml -p 1 -v 1
```

This runs in single-iteration passthru mode with info-level logging.

## Troubleshooting

### `RuntimeError: Bad or missing HDF5_PLUGIN_PATH`

The `HDF5_PLUGIN_PATH` environment variable is not set or does not contain `liblowfive`. See [Environment Setup](../getting-started/environment.md).

### `RuntimeError: The 'lowfive' Python package is required`

The `lowfive` Python bindings are not installed. Install via Spack (`spack install lowfive`) or build from source.

### `RuntimeError: The 'pyhenson' package is required`

The `pyhenson` Python bindings are not installed. Install Henson via Spack (`spack install henson`) or build from source. Only required for C++ tasks.

### Workflow hangs

- Verify the total process count matches the sum of `nprocs * taskCount`.
- Check that all consumers have matching inports for every producer outport (or orphan ports use file mode).
- For cycle topologies, ensure all nodes have both inports and outports with distinct filenames.

### `mutex lock failed: Invalid argument` at exit

This is a known benign issue caused by HDF5/spdlog teardown ordering. Wilkins calls `H5close()` before exit to mitigate it. If you still see it, it does not affect data correctness.
