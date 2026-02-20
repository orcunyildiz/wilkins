# Python API Reference

This page is intended as a reference for developers who need to interact with Wilkins programmatically — for example, when writing custom task codes that require access to MPI communicators, or when building tools on top of the Wilkins workflow engine. Most users will not need to call these APIs directly, since the `wilkins-master` driver handles orchestration automatically. All classes and functions listed here are importable directly from `wilkins`:

```python
from wilkins import Workflow, Wilkins, Dataflow, Comm, LowFiveProperty
from wilkins import get_local_comm, get_intercomms
```

## `Workflow`

**Module:** `wilkins.workflow`

Represents the entire workflow graph (nodes and links). Parsed from YAML.

```python
from wilkins.workflow import Workflow

workflow = Workflow()
workflow.make_wflow_from_yaml("config.yaml")
```

### Constructor

```python
Workflow(nodes=None, links=None)
```

| Parameter | Type | Default | Description |
|---|---|---|---|
| `nodes` | list of `WorkflowNode` | `None` (empty list) | Pre-built workflow nodes. |
| `links` | list of `WorkflowLink` | `None` (empty list) | Pre-built workflow links. |

### Attributes

| Attribute | Type | Description |
|---|---|---|
| `nodes` | list of `WorkflowNode` | All task nodes in the workflow. |
| `links` | list of `WorkflowLink` | All dataflow links (auto-generated from ports). |

### Methods

#### `make_wflow_from_yaml(yaml_path)`

Parse a YAML configuration file and populate the workflow graph.

```python
workflow.make_wflow_from_yaml("config.yaml")
# or equivalently:
Workflow.make_wflow_from_yaml(workflow, "config.yaml")
```

#### `my_node(proc, node) -> bool`

Whether MPI rank `proc` belongs to node index `node`.

#### `my_in_link(proc, link) -> bool`

Whether MPI rank `proc` receives input from link index `link`.

#### `my_out_link(proc, link) -> bool`

Whether MPI rank `proc` sends output on link index `link`.

---

## `WorkflowNode`

**Module:** `wilkins.workflow`

A single task in the workflow graph.

### Attributes

| Attribute | Type | Default | Description |
|---|---|---|---|
| `func` | str | `""` | Task name or script path. |
| `nprocs` | int | `0` | Number of MPI processes. |
| `nwriters` | int | `-1` | Number of writer processes (`-1` = all). |
| `taskCount` | int | `1` | Number of ensemble instances. |
| `start_proc` | int | `0` | Starting MPI rank in `COMM_WORLD`. |
| `args` | list of str | `[]` | Command-line arguments. |
| `actions` | list of str | `[]` | Callback action `[module, function]`. |
| `out_links` | list of int | `[]` | Indices of outgoing links. |
| `in_links` | list of int | `[]` | Indices of incoming links. |
| `l5_outports` | list of `LowFivePort` | `[]` | Output port definitions. |
| `l5_inports` | list of `LowFivePort` | `[]` | Input port definitions. |
| `passthru_files` | list of tuple | `[]` | Orphan port `(filename, dset)` pairs for file I/O. |

---

## `WorkflowLink`

**Module:** `wilkins.workflow`

A dataflow edge connecting a producer node to a consumer node.

### Attributes

| Attribute | Type | Default | Description |
|---|---|---|---|
| `prod` | int | `0` | Producer node index. |
| `con` | int | `0` | Consumer node index. |
| `name` | str | `""` | Link name (`filename/dset:consumer_func`). |
| `fullName` | str | `""` | Full name including producer (`name:producer_func`). |
| `execGroup` | str | `""` | Execution group (`producer_func:consumer_func`). |
| `flow_policy` | int | `1` | Flow control policy (I/O frequency). |
| `tokens` | int | `0` | Empty messages for cycle support. |
| `in_passthru` | int | `0` | Consumer-side file mode flag. |
| `in_metadata` | int | `1` | Consumer-side memory mode flag. |
| `out_passthru` | int | `0` | Producer-side file mode flag. |
| `out_metadata` | int | `1` | Producer-side memory mode flag. |
| `zerocopy` | int | `0` | Zero-copy transfer flag. |

---

## `LowFivePort`

**Module:** `wilkins.workflow`

A single input or output port with LowFive properties.

### Attributes

| Attribute | Type | Default | Description |
|---|---|---|---|
| `name` | str | `""` | Full path: `filename/dataset`. |
| `filename` | str | `""` | HDF5 filename. |
| `dset` | str | `""` | Dataset path within HDF5 file. |
| `zerocopy` | int | `0` | Zero-copy transfer flag. |
| `passthru` | int | `0` | File mode flag. |
| `metadata` | int | `1` | Memory mode flag. |
| `io_freq` | int | `1` | I/O frequency for flow control. |

---

## `Wilkins`

**Module:** `wilkins.wilkins`

Top-level Wilkins interface. Orchestrates workflow parsing, dataflow creation, communicator building, and LowFive property setup.

```python
from wilkins.wilkins import Wilkins
from mpi4py import MPI

wilkins = Wilkins(MPI.COMM_WORLD, "config.yaml")
```

### Constructor

```python
Wilkins(world_comm, config_file)
```

| Parameter | Type | Description |
|---|---|---|
| `world_comm` | mpi4py `Comm` | MPI communicator (typically `MPI.COMM_WORLD`). |
| `config_file` | str | Path to the YAML workflow configuration file. |

### Methods

#### `my_node(name) -> bool`

Whether the current MPI rank belongs to the named workflow node.

```python
if wilkins.my_node("producer"):
    # this rank is part of the producer task
    ...
```

#### `set_lowfive() -> list[LowFiveProperty]`

Return the computed LowFive properties for all dataflow links.

#### `build_intercomms() -> list`

Create MPI intercommunicators for all dataflow links. Returns the list of intercommunicators. This is a **collective** call — it must be called on all ranks.

#### `build_intercomms_shared(task_name) -> list[int]`

For time-partitioned (shared) mode: return a bitmask indicating which intercommunicators belong to the named task.

#### `commit()`

Producer signals that data is ready. Performs a barrier on passthru out-intercommunicators.

#### `wait()`

Consumer blocks until the producer commits. Performs a barrier on passthru in-intercommunicators.

#### `is_io_proc() -> int`

Whether this process participates in LowFive I/O operations. Returns `1` if yes, `0` if this rank is excluded (e.g., non-writer ranks when `nwriters` is set).

#### `local_comm_handle()`

Return the local task MPI communicator (mpi4py `Comm` object).

#### `local_comm_rank() -> int`

Return rank within the local task communicator.

#### `local_comm_size() -> int`

Return size of the local task communicator.

#### `prod_comm_handle()`

Return the producer communicator.

#### `con_comm_handle()`

Return the consumer communicator.

#### `prod_comm_size(i=None) -> int`

Return size of the producer communicator. If `i` is given, returns size for inbound dataflow `i`.

#### `con_comm_size(i=None) -> int`

Return size of the consumer communicator. If `i` is given, returns size for outbound dataflow `i`.

#### `workflow_comm_size() -> int`

Return total workflow size (`COMM_WORLD` size).

#### `workflow_comm_rank() -> int`

Return rank within the workflow.

#### `nb_dataflows() -> int`

Total number of dataflows built by this instance.

---

## `LowFiveProperty`

**Module:** `wilkins.wilkins`

Properties for a single LowFive dataset link, used internally by the driver to configure the VOL plugin.

### Attributes

| Attribute | Type | Default | Description |
|---|---|---|---|
| `filename` | str | `"*"` | HDF5 filename. |
| `dset` | str | `"*"` | Dataset path. |
| `execGroup` | str | `""` | Execution group name. |
| `zerocopy` | int | `0` | Zero-copy flag. |
| `memory` | int | `1` | `0`: passthru, `1`: metadata (memory). |
| `producer` | int | `0` | Whether this is a producer-side property. |
| `consumer` | int | `0` | Whether this is a consumer-side property. |
| `prodIndex` | int | `0` | Producer intercommunicator index. |
| `conIndex` | int | `0` | Consumer intercommunicator index. |
| `flowPolicy` | int | `1` | Flow control policy. |

---

## `Dataflow`

**Module:** `wilkins.dataflow`

Represents a single producer-consumer dataflow link with its MPI communicators.

### Constructor

```python
Dataflow(world_comm, workflow_size, workflow_rank, io_proc, wilkins_sizes, prod, dflow, con, wflow_link)
```

Typically constructed internally by `Wilkins`. Not intended for direct user construction.

### Methods

| Method | Returns | Description |
|---|---|---|
| `sizes()` | `WilkinsSizes` | Producer/consumer sizes and starting ranks. |
| `is_prod()` | bool | Whether this rank is a producer. |
| `is_con()` | bool | Whether this rank is a consumer. |
| `is_prod_root()` | bool | Whether this rank is the producer root. |
| `is_con_root()` | bool | Whether this rank is the consumer root. |
| `prod_comm_handle()` | mpi4py `Comm` | Producer MPI communicator. |
| `con_comm_handle()` | mpi4py `Comm` | Consumer MPI communicator. |
| `in_passthru()` | int | Consumer passthru flag. |
| `in_metadata()` | int | Consumer metadata flag. |
| `out_passthru()` | int | Producer passthru flag. |
| `out_metadata()` | int | Producer metadata flag. |
| `flowPolicy()` | int | Flow control policy. |
| `zerocopy()` | int | Zero-copy flag. |
| `name()` | str | Link name. |
| `execGroup()` | str | Execution group name. |
| `fullName()` | str | Full link name with source. |

---

## `Comm`

**Module:** `wilkins.comm`

MPI communicator wrapper that creates sub-communicators from rank ranges.

### Constructor

```python
Comm(world_comm, lo=None, hi=None)
```

| Parameter | Type | Description |
|---|---|---|
| `world_comm` | mpi4py `Comm` | Parent MPI communicator. |
| `lo` | int or None | Lowest rank to include. If `None`, wraps the communicator directly. |
| `hi` | int or None | Highest rank to include (inclusive). |

### Methods

| Method | Returns | Description |
|---|---|---|
| `handle()` | mpi4py `Comm` | The underlying MPI communicator. |

---

## `WilkinsSizes`

**Module:** `wilkins.types`

Sizes and starting ranks for producer/consumer communicators.

### Attributes

| Attribute | Type | Default | Description |
|---|---|---|---|
| `prod_size` | int | `0` | Number of producer processes. |
| `prod_writers` | int | `-1` | Number of writer processes (`-1` = all). |
| `con_size` | int | `0` | Number of consumer processes. |
| `prod_start` | int | `0` | Starting world rank of the producer. |
| `con_start` | int | `0` | Starting world rank of the consumer. |

---

## Module-level functions

### `get_local_comm(wilkins) -> Comm`

**Module:** `wilkins.wilkins`

Return the local task communicator as an mpi4py communicator.

```python
from wilkins.wilkins import Wilkins, get_local_comm
comm = get_local_comm(wilkins)
```

### `get_intercomms(wilkins) -> list`

**Module:** `wilkins.wilkins`

Build and return the intercommunicators for this task. **Collective** — must be called on all ranks.

```python
from wilkins.wilkins import Wilkins, get_intercomms
intercomms = get_intercomms(wilkins)
```

### `comm_rank(comm) -> int`

**Module:** `wilkins.comm`

Return the rank of the calling process in the given communicator.

### `comm_size(comm) -> int`

**Module:** `wilkins.comm`

Return the size of the given communicator.

---

## Constants

**Module:** `wilkins.types`

| Constant | Value | Description |
|---|---|---|
| `WILKINS_OTHER_COMM` | `0x00` | Rank is not part of producer or consumer. |
| `WILKINS_PRODUCER_COMM` | `0x01` | Rank is a producer. |
| `WILKINS_CONSUMER_COMM` | `0x04` | Rank is a consumer. |
