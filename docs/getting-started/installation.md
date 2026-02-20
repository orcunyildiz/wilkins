# Installation

## Prerequisites

Wilkins requires the following runtime dependencies:

- **Python 3.8+**
- **MPI** implementation (e.g., [MPICH](https://www.mpich.org))
- **[mpi4py](https://mpi4py.readthedocs.io/)** — Python bindings for MPI
- **[LowFive](https://github.com/diatomic/LowFive)** — HDF5 VOL plugin for in situ data transport (with Python bindings)
- **[HDF5](https://www.hdfgroup.org/solutions/hdf5/)** version 1.14
- **[Henson](https://github.com/henson-insitu/henson)** — coroutine-based cooperative multitasking (Python bindings required only for C++ task codes)
- **[PyYAML](https://pyyaml.org/)** — YAML parser (installed automatically with pip)
- **[h5py](https://www.h5py.org/)** — HDF5 for Python (needed by task codes that read/write HDF5 data)

## Installing with Spack (recommended)

[Spack](https://spack.readthedocs.io/) is the recommended installation method because it handles all native dependencies (MPI, HDF5, LowFive, Henson) automatically.

### Step 1 — Install Spack

Follow the [Spack getting started guide](https://spack.readthedocs.io/en/latest/getting_started.html).

### Step 2 — Register the Wilkins and LowFive repos

Wilkins and LowFive each ship their own Spack package recipes. Clone both repositories and register them as Spack repos:

```bash
# Wilkins
git clone https://github.com/orcunyildiz/wilkins.git /path/to/wilkins
spack repo add /path/to/wilkins

# LowFive
git clone https://github.com/diatomic/LowFive.git /path/to/lowfive
spack repo add /path/to/lowfive
```

Verify that Spack can find both packages:

```bash
spack info wilkins
spack info lowfive
```

### Step 3 — Install

```bash
spack install wilkins
```

This may take some time on a fresh Spack installation because all dependencies (MPI, HDF5, etc.) are built from source by default. Experienced Spack users can configure [external packages](https://spack.readthedocs.io/en/latest/build_settings.html#external-packages) to speed this up.

### Step 4 — Load

```bash
spack load wilkins
```

This sets up all required environment variables, including `HDF5_PLUGIN_PATH` and `HDF5_VOL_CONNECTOR` (see [Environment Setup](environment.md)).

## Installing with pip

If you already have all native dependencies (MPI, HDF5, LowFive, Henson) installed on your system, you can install the Wilkins Python package with pip.

From the root of the Wilkins repository:

```bash
pip install .
```

Or for development (editable install):

```bash
pip install -e .
```

This installs the `wilkins` Python package and the `wilkins-master` command-line tool.

:::{note}
pip only installs the Python dependencies (`mpi4py`, `pyyaml`). You must ensure the native libraries (MPI, HDF5, LowFive, Henson) are available separately, either by building them from source or installing them via Spack.
:::

## Verifying the installation

After installation, verify that Wilkins is importable:

```bash
python -c "import wilkins; print('Wilkins installed successfully')"
```

You should also verify that the `wilkins-master` console script is available:

```bash
wilkins-master --help
```
