# LowFive Examples

C++ task examples orchestrated by the Python Wilkins workflow system using
[LowFive](https://github.com/diatomic/LowFive) for in-memory HDF5 data
transport and [Henson](https://github.com/henson-insern/henson) for
cooperative task execution.

## Prerequisites

1. **Python Wilkins** (the orchestrator):

   ```bash
   # from the repository root
   pip install .
   ```

2. **Runtime dependencies** (install via [Spack](https://spack.io/) or from
   source):

   | Package | Purpose |
   |---------|---------|
   | MPI (e.g. MPICH, Open MPI) | Process management |
   | HDF5 (parallel build) | Data I/O |
   | [LowFive](https://github.com/diatomic/LowFive) | HDF5 VOL plugin for in-memory transport |
   | [Henson](https://github.com/henson-insern/henson) | Cooperative multitasking (loads `.hx` libs) |
   | [DIY](https://github.com/diatomic/diy) | Domain decomposition (bundled in `ext/diy`) |
   | [fmt](https://github.com/fmtlib/fmt) | Formatted output (bundled in `ext/fmt`) |
   | mpi4py | Python MPI bindings (`pip install mpi4py`) |
   | pyhenson | Python bindings for Henson |
   | lowfive (Python) | Python bindings for LowFive |

3. **Environment variables**:

   ```bash
   export HDF5_PLUGIN_PATH=/path/to/lowfive/build/src   # directory containing liblowfive.so
   export HDF5_VOL_CONNECTOR="lowfive under_vol=0;under_info={};"
   ```

## Building the C++ Task Libraries

The C++ examples are built with CMake as part of the main project:

```bash
mkdir build && cd build
cmake .. -Dlowfive=ON -Dwilkins_python=OFF \
         -DHENSON_LIBRARY=/path/to/libhenson.so \
         -DHENSON_PMPI_LIBRARY=/path/to/libhenson-pmpi.so \
         -DLOWFIVE_LIBRARY=/path/to/liblowfive.so \
         -DLOWFIVE_DIST_LIBRARY=/path/to/liblowfive-dist.so
make -j
```

This produces `.hx` shared libraries (e.g., `sim.hx`, `ana.hx`, `node0.hx`,
`prod.hx`) in the build tree under `examples/lowfive/`.

> **Note:** The `-Dwilkins_python=OFF` flag is recommended because the old
> pybind11 bindings are no longer needed.  Wilkins is now a pure-Python
> package installed separately via `pip install .`.

## Running Examples

All examples use the `wilkins-master` command (installed by `pip install .`)
to orchestrate the workflow.  The general pattern is:

```bash
mpirun -n <nprocs> -l wilkins-master <config>.yaml [options]
```

Options:
- `-p 0|1` -- passthru support (0: none [default], 1: single iteration)
- `-v 0|1|2` -- verbosity (0: none [default], 1: info, 2: debug)

Alternatively, you can use `python -m wilkins.master` in place of
`wilkins-master`.

Copy or symlink the `.hx` libraries into the directory containing the YAML
config before running.

---

### 1. Flow Control -- Stateless

A producer (`sim.hx`) writes grid and particle data; a consumer (`ana.hx`)
reads it.  The consumer is stateless and is re-launched each time by the
framework.  The `io_freq` setting in the YAML config controls how often the
consumer receives data.

```bash
cd build/examples/lowfive/flow-control/stateless/

# Different files per timestep (default)
mpirun -n 2 -l wilkins-master wilkins_prod_con.yaml

# Same file every timestep
mpirun -n 2 -l wilkins-master wilkins_prod_con_singleFile.yaml

# Or use the convenience script:
bash run_stateless.sh              # different files
bash run_stateless.sh -single      # single file
```

**YAML configs:**
- `wilkins_prod_con.yaml` -- wildcard filenames (`*.h5`), `io_freq: 2`
- `wilkins_prod_con_singleFile.yaml` -- fixed filename, `io_freq: 2`

---

### 2. Flow Control -- Stateful

Producer and consumer tasks loop internally with flow control policies.

#### Producer-Consumer (1:1)

```bash
cd build/examples/lowfive/flow-control/stateful/

mpirun -n 2 -l wilkins-master wilkins_prod_con.yaml

# Or use the convenience script:
bash run_stateful.sh
```

#### Producer with Two Consumers (1:2)

One producer feeds two consumers reading different datasets (`grid` vs
`particles`) with different flow control policies (`io_freq: -1` for
demand-driven, `io_freq: 3` for periodic).

```bash
cd build/examples/lowfive/flow-control/stateful/

mpirun -n 3 -l wilkins-master wilkins_prod_2cons.yaml
```

**YAML configs:**
- `wilkins_prod_con.yaml` -- 1 producer, 1 consumer (passthru mode)
- `wilkins_prod_con_singleFile.yaml` -- single file variant
- `wilkins_prod_2cons.yaml` -- 1 producer, 2 consumers with mixed flow control

---

### 3. Cyclic Dataflow

Three nodes forming a cycle: `node0 -> node1 -> node2 -> node0`.

- `node0.hx`: writes grid + particles; reads particles back from node2
  (starting at iteration 1)
- `node1.hx`: reads from node0, writes particles
- `node2.hx`: reads from node1, writes reduced particles back to node0

```bash
cd build/examples/lowfive/cycle/

mpirun -n 3 -l wilkins-master wilkins_cycle.yaml

# Or:
bash run_cycle.sh
```

---

### 4. Ensembles

Multiple instances of the same task type running concurrently.  Three
topology variants are provided:

| Config | Topology | Producers | Consumers |
|--------|----------|-----------|-----------|
| `NxN.yaml` | N-to-N | 2 | 2 (stateful) |
| `fanin.yaml` | Fan-in | 2 | 1 (stateful) |
| `fanout.yaml` | Fan-out | 2 | 4 (stateless) |

Ensemble runs require per-instance copies of `.hx` libraries.  The
`generateRunScript.sh` helper automates this:

```bash
cd build/examples/lowfive/ensembles/

# Generate per-instance .hx copies and the run script
bash generateRunScript.sh NxN.yaml

# Execute
bash run_ensemble.sh
```

For fan-in:
```bash
bash generateRunScript.sh fanin.yaml
bash run_ensemble.sh
```

For fan-out:
```bash
bash generateRunScript.sh fanout.yaml
bash run_ensemble.sh
```

---

### 5. Python Tasks (Henson mode)

Pure-Python producer and consumer tasks orchestrated by Wilkins via Henson.

```bash
cd examples/python/

mpirun -n 2 -l wilkins-master wilkins_prod_con.yaml

# Or:
bash wilkins_run.sh
```

- `producer.py` -- writes a NumPy array to `particles.h5` via h5py
- `consumer.py` -- reads and prints the array

---

### 6. Python Tasks (MPMD mode)

Tasks launched as separate MPI programs (no Henson).  Each task creates its
own `Wilkins` object and uses `commit()`/`wait()` for synchronization.

```bash
cd examples/python/mpmd/

mpirun -np 3 -l python ./prod.py : -np 1 python ./con.py

# Or:
bash wilkins_prod_con.sh
```

---

## Generating Run Scripts

The `generate_run_script.py` utility can create MPI, SLURM, or PBS batch
scripts from any YAML config:

```bash
# MPI (default)
python generate_run_script.py <config>.yaml

# SLURM
python generate_run_script.py <config>.yaml --scheduler slurm --walltime 02:00:00

# PBS
python generate_run_script.py <config>.yaml --scheduler pbs --cores-per-node 64

# All schedulers at once
python generate_run_script.py <config>.yaml --all
```

---

## YAML Configuration Reference

Each workflow is defined by a YAML config file.  A minimal example:

```yaml
tasks:
  - func: "./sim.hx"          # path to task library or script
    nprocs: 1                  # number of MPI ranks
    args: ["6"]                # command-line arguments passed to the task
    outports:
      - filename: "*.h5"      # output file pattern (* = wildcard)
        dsets:
          - name: /group1/grid
            passthru: 0        # 0 = in-memory (LowFive), 1 = file-based
            metadata: 1        # 1 = metadata in memory
  - func: "./ana.hx"
    nprocs: 1
    args: ["{filename}"]       # {filename} is replaced at runtime
    inports:
      - filename: "*.h5"
        io_freq: 2             # consumer receives every 2nd producer output
        dsets:
          - name: /group1/grid
            passthru: 0
            metadata: 1
```

Key fields:
- `func`: path to `.hx` shared library or `.py` script
- `nprocs`: number of MPI processes for this task
- `start_proc`: (optional) explicit starting rank
- `args`: (optional) list of command-line arguments
- `taskCount`: (optional) number of ensemble instances (default: 1)
- `outports` / `inports`: data ports with file/dataset specifications
- `io_freq`: (on inports) flow control frequency (-1 = demand-driven)
- `passthru`: 0 = in-memory via LowFive, 1 = file-based I/O
- `metadata`: 1 = keep metadata in memory
- `zerocopy`: 1 = zero-copy data transfer (producer side only)
- `actions`: (optional) `["module_name", "function_name"]` for callbacks
- `nwriters`: (optional) number of writer ranks (subset of nprocs)
