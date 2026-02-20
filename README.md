# Wilkins

Wilkins is an in situ workflow system that enables heterogeneous task specification and execution for in situ data processing. It provides a data-centric API for defining workflow graphs, creating and launching tasks, and establishing communicators between them.

As its data transport layer, Wilkins uses [LowFive](https://github.com/diatomic/LowFive), an HDF5 VOL plugin that allows coupled tasks to communicate both in situ using in-memory data and MPI message passing, and through traditional HDF5 files. Minimal and often no source-code modification is needed for programs that already use HDF5.

Wilkins supports any directed-graph topology of tasks, including pipeline, fan-in, fan-out, ensembles, and cycles.

## Quick start

```bash
# Install
pip install .

# Set up environment
export HDF5_VOL_CONNECTOR="lowfive under_vol=0;under_info={};"
export HDF5_PLUGIN_PATH=/path/to/lowfive/build/src

# Run a workflow
mpirun -n 2 wilkins-master config.yaml
```

With Spack:

```bash
spack install wilkins
spack load wilkins
mpirun -n 2 wilkins-master config.yaml
```

## Documentation

Full documentation is available at **[wilkins.readthedocs.io](https://wilkins.readthedocs.io/)**, including:

- [Installation guide](https://wilkins.readthedocs.io/en/latest/getting-started/installation.html)
- [Quickstart tutorial](https://wilkins.readthedocs.io/en/latest/getting-started/quickstart.html)
- [YAML configuration reference](https://wilkins.readthedocs.io/en/latest/user-guide/yaml-reference.html)
- [Workflow topologies](https://wilkins.readthedocs.io/en/latest/user-guide/topologies.html)
- [Python API reference](https://wilkins.readthedocs.io/en/latest/reference/python-api.html)

## Example

A minimal producer-consumer workflow:

**producer.py**
```python
import h5py
import numpy as np

def main():
    f = h5py.File('particles.h5', 'w')
    f.create_dataset("data", data=np.ones((4, 3, 2), 'f'))
    f.close()
```

**consumer.py**
```python
import h5py

def main():
    f = h5py.File("particles.h5", "r")
    data = f["data"][:]
    print(data)
    f.close()
```

**config.yaml**
```yaml
tasks:
  - func: "./producer.py"
    nprocs: 1
    outports:
      - filename: "particles.h5"
        dsets:
          - name: "*"
            metadata: 1
  - func: "./consumer.py"
    nprocs: 1
    inports:
      - filename: "particles.h5"
        dsets:
          - name: "*"
            metadata: 1
```

```bash
mpirun -n 2 wilkins-master config.yaml
```

## Citation

If you use Wilkins in your research, please cite:

> Yildiz, O., Morozov, D., Nigmetov, A., Nicolae, B., and Peterka, T. (2024). Wilkins: HPC in situ workflows made easy. *Frontiers in High Performance Computing*, 2, 1472719. doi: [10.3389/fhpcp.2024.1472719](https://doi.org/10.3389/fhpcp.2024.1472719)

## License

BSD-3-Clause
