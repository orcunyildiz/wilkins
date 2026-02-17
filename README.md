# Wilkins
Wilkins is an in situ workflow system that enables heterogenous task specification and execution for in situ data processing.
Wilkins provides a data-centric API for defining the workflow graph, creating and launching tasks, establishing communicators between the tasks. 
As its data transport layer, Wilkins uses [LowFive](https://github.com/diatomic/LowFive) library, which is based on the [HDF5](https://www.hdfgroup.org/solutions/hdf5/) data model.
Wilkins allows coupled tasks to communicate both in situ using in-memory data and MPI message passing, and through traditional HDF5 files.
Minimal and often no source-code modification is needed for programs that already use HDF5.
Wilkins supports any directed-graph topology of tasks, including common patterns such as pipeline, fan-in, fan-out, ensembles of tasks, and cycles.

# Installation

## Prerequisites

Wilkins requires the following runtime dependencies:

- Python 3.8 or higher
- [MPI](http://www.mpich.org) (e.g., MPICH or Open MPI)
- [mpi4py](https://mpi4py.readthedocs.io/)
- [LowFive](https://github.com/diatomic/LowFive) (Python bindings)
- [HDF5](https://www.hdfgroup.org/solutions/hdf5/) version 1.14
- [Henson](https://github.com/henson-insitu/henson) (Python bindings -- only required for C++ task codes)

## Installing with pip

```bash
pip install .
```

Or for development (editable install):

```bash
pip install -e .
```

This installs the `wilkins` Python package and the `wilkins-master` command-line tool.

## Installing with Spack

First, install Spack as explained [here](https://spack.readthedocs.io/en/latest/getting_started.html). Once Spack is
installed and available in your path, clone the Wilkins and LowFive repositories and add them to your local Spack repositories:

```
cd /path/to/wilkins/
git clone https://github.com/orcunyildiz/wilkins.git .
spack repo add /path/to/wilkins/

cd /path/to/lowfive/
git clone https://github.com/diatomic/LowFive.git .
spack repo add /path/to/lowfive/
```

You can confirm that Spack can find Wilkins and LowFive:
```
spack info wilkins
spack info lowfive
```

Then install Wilkins. This could take some time depending on whether you already have a Spack system with MPI
installed. The first time you use Spack, many dependencies need to be satisfied, which by default are installed from
scratch. If you are an experienced Spack user, you can tell Spack to use existing dependencies from
elsewhere in your system.

```
spack install wilkins
```

## Verifying the installation

After installation, verify that Wilkins is importable:

```bash
python -c "import wilkins; print('Wilkins installed successfully')"
```

# Environment setup

Wilkins uses LowFive as its data transport layer. Before running workflows, set the following environment variables:

```bash
export HDF5_VOL_CONNECTOR="lowfive under_vol=0;under_info={};"
export HDF5_PLUGIN_PATH=/path/to/lowfive/build/src
```

With Spack installation, these are automatically set after doing `spack load wilkins`.

# Running examples

Wilkins provides several examples of simple workflows.

```bash
# Run a cycle example
cd /path/to/wilkins/examples/lowfive/cycle
./run_cycle.sh

# Run a flow control example
cd /path/to/wilkins/examples/lowfive/flow-control/stateful
./run_stateful.sh
```

# Usage

## Running a workflow

After installing Wilkins and setting up the environment, run a workflow using:

```bash
mpirun -n <nprocs> wilkins-master config.yaml
```

Or equivalently:

```bash
mpirun -n <nprocs> python -m wilkins.master config.yaml
```

### Command-line options

```
wilkins-master config.yaml [-p 0|1] [-v 0|1|2]

  -p, --passthruSupport  Passthru support level (0: none [default], 1: single iteration)
  -v, --verbosity        Logging level (0: none [default], 1: info, 2: debug)
```

## Using Wilkins in your own project

To execute user task codes with Wilkins, you need to:

1. **For Python tasks**: No compilation needed. Write your task as a Python script with a `main()` function and reference the `.py` file in the YAML configuration.

2. **For C++ tasks**: Link them with [Henson](https://github.com/henson-insitu/henson/) and compile as shared objects (`.hx` files). The task codes need to be compiled as position-independent codes (`-fPIE`). On Linux, add `-pie -Wl,--export-dynamic` and `-Wl,-u,henson_set_contexts,-u,henson_set_namemap` as linker flags.

3. **Create a YAML configuration file** describing the workflow tasks, their data requirements, and the number of MPI processes per task.

## Workflow configuration (YAML)

Below is a sample YAML file for a 3-task workflow (1 producer, 2 consumers):

```yaml
tasks:
  - func: producer
    nprocs: 3
    outports:
      - filename: outfile.h5
        dsets:
          - name: /group1/grid
            file: 0
            memory: 1
          - name: /group1/particles
            file: 0
            memory: 1
  - func: consumer1
    nprocs: 5
    inports:
      - filename: outfile.h5
        dsets:
          - name: /group1/grid
            file: 0
            memory: 1
  - func: consumer2
    nprocs: 2
    inports:
      - filename: outfile.h5
        dsets:
          - name: /group1/particles
            file: 0
            memory: 1
```

## Python API

The Wilkins Python package can also be used programmatically:

```python
from wilkins.workflow import Workflow
from wilkins.wilkins import Wilkins, get_local_comm, get_intercomms
from mpi4py import MPI

# Parse workflow configuration
workflow = Workflow()
workflow.make_wflow_from_yaml("config.yaml")

# Create Wilkins instance
wilkins = Wilkins(MPI.COMM_WORLD, "config.yaml")

# Get local communicator and intercommunicators
comm = get_local_comm(wilkins)
intercomms = get_intercomms(wilkins)

# Get LowFive properties
l5_props = wilkins.set_lowfive()
```

# Project structure

```
wilkins/
  bindings/python/wilkins/    # Pure-Python Wilkins package
    __init__.py               # Package init with public API
    types.py                  # Type definitions (WilkinsSizes, constants)
    comm.py                   # MPI communicator wrapper
    context.py                # Global state management
    workflow.py               # Workflow graph + YAML parser
    dataflow.py               # Single dataflow link
    wilkins.py                # Top-level Wilkins class
    master.py                 # Workflow driver (wilkins-master entry point)
    utils.py                  # Utility functions
    pywilkins.py              # Backwards-compatibility shim
  examples/                   # Example workflows
  pyproject.toml              # Python package configuration
  setup.py                    # Setuptools shim for editable installs
```

# Legacy C++ build (deprecated)

The original C++ implementation and CMake build system are retained in `src/`,
`include/`, and `CMakeLists.txt` for reference. The pure-Python port in
`bindings/python/wilkins/` replaces the C++ library (`libwilkins`) and the
pybind11 extension module (`pywilkins`). New users should use `pip install`
instead of CMake.
