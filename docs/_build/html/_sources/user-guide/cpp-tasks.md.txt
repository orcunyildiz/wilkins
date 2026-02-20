# Writing C++ Tasks

C++ tasks are compiled as shared objects (`.so` or `.hx` files) and run as coroutines managed by [Henson](https://github.com/henson-insitu/henson). This is the path for integrating existing C/C++ simulation or analysis codes into a Wilkins workflow.

## Prerequisites

- [Henson](https://github.com/henson-insitu/henson) — coroutine-based cooperative multitasking library
- `pyhenson` — Henson's Python bindings (required by the Wilkins driver)
- HDF5 C library (not h5py)

All are installable via Spack:

```bash
spack install wilkins   # pulls in henson, lowfive, hdf5 automatically
```

## Compilation

C++ task codes must be compiled as **position-independent shared objects** with specific linker flags.

:::{note}
The linker flags differ between Linux and macOS. On Linux, the `--export-dynamic` and `-u` flags are required for Henson symbol resolution. On macOS, these flags are not needed because the dynamic linker handles symbol visibility differently. The examples below show the correct flags for each platform.
:::

### Linux

```bash
g++ -fPIE -pie -Wl,--export-dynamic \
    -Wl,-u,henson_set_contexts,-u,henson_set_namemap \
    -o my_task.hx my_task.cpp \
    -lhenson -lhdf5
```

### macOS

```bash
clang++ -fPIE \
    -o my_task.hx my_task.cpp \
    -lhenson -lhdf5
```

### CMake

If using CMake, a typical target definition:

```cmake
add_library(my_task SHARED my_task.cpp)
set_target_properties(my_task PROPERTIES
    SUFFIX ".hx"
    PREFIX ""
    POSITION_INDEPENDENT_CODE ON
)
target_link_libraries(my_task henson hdf5)

if(UNIX AND NOT APPLE)
    target_link_options(my_task PRIVATE
        -pie -Wl,--export-dynamic
        -Wl,-u,henson_set_contexts,-u,henson_set_namemap
    )
endif()
```

## Task structure

A C++ task uses the HDF5 C API directly. LowFive intercepts these calls at runtime, just as it does for h5py in Python tasks.

### Producer

```c
#include <hdf5.h>

int main(int argc, char* argv[])
{
    // Create/open HDF5 file — LowFive intercepts this
    hid_t file = H5Fcreate("outfile.h5", H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

    // Create dataset and write data
    hsize_t dims[2] = {10, 3};
    hid_t space = H5Screate_simple(2, dims, NULL);
    hid_t dset = H5Dcreate(file, "/group1/grid", H5T_NATIVE_FLOAT,
                           space, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    float data[10][3];
    // ... fill data ...
    H5Dwrite(dset, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, data);

    H5Dclose(dset);
    H5Sclose(space);
    H5Fclose(file);

    return 0;
}
```

### Consumer

```c
#include <hdf5.h>

int main(int argc, char* argv[])
{
    hid_t file = H5Fopen("outfile.h5", H5F_ACC_RDONLY, H5P_DEFAULT);
    hid_t dset = H5Dopen(file, "/group1/grid", H5P_DEFAULT);

    float data[10][3];
    H5Dread(dset, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, data);

    // ... process data ...

    H5Dclose(dset);
    H5Fclose(file);

    return 0;
}
```

## YAML configuration

Reference the shared object in the `func` field:

```yaml
tasks:
  - func: "./producer.hx"
    nprocs: 2
    args: ["5"]
    outports:
      - filename: "outfile.h5"
        dsets:
          - name: /group1/grid
            metadata: 1
          - name: /group1/particles
            metadata: 1
  - func: "./consumer.hx"
    nprocs: 1
    args: ["5", "{filename}"]
    inports:
      - filename: "outfile.h5"
        dsets:
          - name: /group1/grid
            metadata: 1
          - name: /group1/particles
            metadata: 1
```

The `args` list is passed to the task's `main(argc, argv)`. The `{filename}` placeholder is replaced at runtime by LowFive with the actual filename.

## Henson coroutines

Wilkins uses Henson to run C++ tasks as coroutines within a single MPI job. Henson provides:

- **ProcMap** — maps task names to MPI process ranges
- **NameMap** — shared key-value store for inter-task communication

The Wilkins driver (`wilkins-master`) creates these automatically from the YAML configuration. Task codes do not need to interact with Henson directly — they just use standard `main()` and HDF5 calls.

## Mixing Python and C++ tasks

A workflow can mix Python and C++ tasks freely. For example:

```yaml
tasks:
  - func: "./simulation.hx"    # C++ producer
    nprocs: 4
    outports:
      - filename: "output.h5"
        dsets:
          - name: "*"
            metadata: 1
  - func: "./analysis.py"      # Python consumer
    nprocs: 2
    inports:
      - filename: "output.h5"
        dsets:
          - name: "*"
            metadata: 1
```

The data transport is identical regardless of language — LowFive handles the interception on both sides.
