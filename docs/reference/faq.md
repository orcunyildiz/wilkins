# FAQ

## Installation

### Do I need to install LowFive and Henson separately?

If you install via **Spack** (`spack install wilkins`), all dependencies are handled automatically.

If you install via **pip** (`pip install .`), only the Python dependencies (`mpi4py`, `pyyaml`) are installed. You must install LowFive (with Python bindings) and Henson (with `pyhenson`) separately — typically via Spack or from source.

### Which HDF5 version do I need?

Wilkins requires HDF5 **1.14** or later, which introduced the VOL plugin mechanism that LowFive relies on.

### Does Wilkins work on macOS?

Yes. Wilkins automatically sets the Python multiprocessing start method to `"fork"` on macOS to work around a known compatibility issue. All other functionality is the same as on Linux.

## Configuration

### How do I know how many MPI processes to use?

The total process count for `mpirun -n` must equal the sum of `nprocs * taskCount` across all tasks in your YAML file. See [Running Workflows](../user-guide/running.md) for details.

### Can I use wildcards in dataset names?

Yes. Use `name: "*"` to match all datasets in a file. The `*` and `?` wildcard characters are supported, following standard glob semantics.

### What happens if I set both `metadata: 0` and `passthru: 0`?

Wilkins will print an error and exit. At least one transport mode must be enabled for each dataset.

### Can different datasets on the same link use different transport modes?

Yes. Each dataset has its own `metadata` and `passthru` flags. You can have some datasets in memory mode and others in file mode within the same workflow.

### What does `{filename}` do in `args`?

The `{filename}` placeholder is replaced at runtime by LowFive with the actual filename of the data being served. This is useful for stateful consumers that process multiple files over time.

## Data Transport

### What is the difference between memory mode and file mode?

- **Memory mode** (`metadata: 1, passthru: 0`): Data is transferred between tasks via MPI. No files are written to disk. This is the default and preferred mode for in situ workflows.
- **File mode** (`metadata: 0, passthru: 1`): Data is written to and read from real HDF5 files on disk. Requires [callback actions](../user-guide/custom-actions.md) for synchronization.

See [Data Transport](../concepts/data-transport.md) for details.

### Do I need to modify my HDF5 code to work with Wilkins?

In most cases, **no**. LowFive intercepts standard HDF5 calls (via h5py in Python or the HDF5 C API in C++) transparently. Your code uses `h5py.File()` or `H5Fcreate()`/`H5Fopen()` as usual.

### What is zero-copy mode?

Zero-copy (`zerocopy: 1`) shares the producer's data buffer pointer with the consumer instead of making a deep copy. This eliminates copy overhead but requires that the producer and consumer share the same address space and that the producer's buffer remains valid until the consumer finishes reading.

## Execution

### Can I mix Python and C++ tasks?

Yes. A workflow can freely mix Python (`.py`) and C++ (`.so`/`.hx`) tasks. LowFive handles data transport identically for both.

### What is the consumer loop?

In master mode, `wilkins-master` manages a consumer loop that repeatedly checks for new data from the producer. Each time data is available, the consumer's `main()` function is called again. This continues until the producer signals completion. Task codes do not need to implement any loop logic.

### Why does my workflow hang?

Common causes:

1. **Wrong process count.** The `-n` argument to `mpirun` must exactly match the sum of `nprocs * taskCount`.
2. **Missing matching ports.** Every inport must have a matching outport (same filename/dataset) or use file mode as an orphan port.
3. **Missing actions in file mode.** Passthru workflows require callback actions for synchronization.
4. **Cycle deadlock.** In cycle topologies, each node must have both inports and outports with distinct filenames per link.

## Troubleshooting

### `RuntimeError: Bad or missing HDF5_PLUGIN_PATH`

Set the `HDF5_PLUGIN_PATH` environment variable to the directory containing `liblowfive.so` (or `.dylib` on macOS). See [Environment Setup](../getting-started/environment.md).

### `mutex lock failed: Invalid argument` at exit

This is a benign teardown ordering issue between HDF5 and spdlog. Wilkins mitigates it by calling `H5close()` before exit. It does not affect data correctness or workflow results.

### `WARNING: Passthru is not enabled at the producer side`

The consumer requested passthru mode, but the producer does not enable it for that dataset. Wilkins automatically falls back to memory mode. To fix, enable `passthru: 1` on the matching producer outport.

### `ERROR: No matching link found for the inport ... requesting memory mode`

An inport has `metadata: 1` but no producer outport matches it. Memory mode requires a matching producer. Either add a matching outport or switch to file mode (`passthru: 1, metadata: 0`) for reading from disk.
