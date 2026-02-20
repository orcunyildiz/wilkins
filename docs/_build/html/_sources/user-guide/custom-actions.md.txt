# Custom Actions

In most Wilkins workflows, data transport and synchronization are handled automatically by LowFive's memory mode. However, when workflows use file mode (passthru) — where tasks communicate through actual HDF5 files on disk — explicit synchronization between producers and consumers becomes necessary. Custom actions provide a mechanism for this by allowing you to register callback functions with the LowFive VOL plugin.

## When are actions needed?

In **memory mode** (`metadata: 1, passthru: 0`), LowFive handles all synchronization automatically. No actions are needed.

In **file mode** (`metadata: 0, passthru: 1`), the producer writes real files to disk and the consumer reads them. Actions are required to:

1. Signal the consumer when a file is ready to read (producer callback)
2. Block the consumer until data is available (consumer callback)

## Configuring actions in YAML

Actions are specified as a two-element list: `["module_name", "function_name"]`:

```yaml
tasks:
  - func: "./producer.hx"
    nprocs: 2
    actions: ["passthru-actions", "prod_callback"]
    outports:
      - filename: "*.h5"
        dsets:
          - name: /group1/grid
            passthru: 1
            metadata: 0
  - func: "./consumer.hx"
    nprocs: 1
    actions: ["passthru-actions", "con_callback"]
    inports:
      - filename: "*.h5"
        io_freq: 1
        dsets:
          - name: /group1/grid
            passthru: 1
            metadata: 0
```

- **`module_name`** — Name of a Python module (without `.py`) that must be importable at runtime. Place it in the working directory or on `PYTHONPATH`.
- **`function_name`** — Name of a callable in that module.

## Built-in passthru actions

Wilkins provides a standard passthru actions module at `examples/lowfive/actions/passthru-actions.py`. Copy it to your working directory and reference it as shown above.

The module defines two callbacks:

```python
from wilkins.utils import setup_passthru_callbacks

def prod_callback(vol, rank):
    setup_passthru_callbacks(vol, "producer")

def con_callback(vol, rank, pl_con):
    setup_passthru_callbacks(vol, "consumer", pl_con)
```

### Producer callback

`setup_passthru_callbacks(vol, "producer")` registers:

- **`set_send_filename`** — After the producer closes an HDF5 file, calls `vol.serve_all(True, False)` to notify consumers that a new file is available.
- **`set_keep`** — Keeps the LowFive metadata alive for the file.

### Consumer callback

`setup_passthru_callbacks(vol, "consumer", pl_con)` registers:

- **`set_before_file_open`** — Before the consumer opens an HDF5 file, retrieves the available filenames from the producer via `vol.get_filenames()` and signals readiness via `vol.send_done()`.

## Writing custom callbacks

You can write your own action module for advanced use cases. The callback function signature is:

```python
def my_callback(vol, rank, pl_con=None):
    """
    :param vol: LowFive DistMetadataVOL object
    :param rank: local MPI rank within the task
    :param pl_con: dict mapping filenames to consumer indices (only for consumers)
    """
    ...
```

Wilkins first tries to call `callback(vol, rank, pl_con)`. If that raises a `TypeError` (e.g., because your callback doesn't accept `pl_con`), it falls back to `callback(vol, rank)`.

### Example: custom write trigger

This example from `examples/lowfive/actions/actions.py` triggers `serve_all` every 4th dataset write instead of on file close:

```python
dw_counter = 0

def callback(vol, rank):
    def adw_cb():
        global dw_counter
        dw_counter = dw_counter + 1
        if dw_counter % 4 == 0:
            print("calling adw callback")
            vol.serve_all(True, True)
    vol.set_after_dataset_write(adw_cb)
    vol.serve_on_close = False
```

YAML:

```yaml
tasks:
  - func: freeze-henson
    nprocs: 3
    nwriters: 1
    actions: ["actions", "callback"]
    outports:
      - filename: dump_h5md.h5
        dsets:
          - name: /particles/all/position/value
            metadata: 1
```

## Cycle topologies with actions

In cycle topologies using file mode, every node is both a producer and a consumer. Wilkins automatically registers both producer and consumer callbacks when it detects a cycle with passthru links. You still need to specify the `actions` field in the YAML for at least the synchronization callbacks:

```yaml
tasks:
  - func: "./node0.hx"
    nprocs: 1
    actions: ["passthru-actions", "prod_callback"]
    inports:
      - filename: "outfile2.h5"
        dsets:
          - name: /group1/particles
            passthru: 1
            metadata: 0
    outports:
      - filename: "outfile0.h5"
        dsets:
          - name: /group1/grid
            passthru: 1
            metadata: 0
  - func: "./node1.hx"
    nprocs: 1
    actions: ["passthru-actions", "con_callback"]
    inports:
      - filename: "outfile0.h5"
        dsets:
          - name: /group1/grid
            passthru: 1
            metadata: 0
    outports:
      - filename: "outfile1.h5"
        dsets:
          - name: /group1/particles
            passthru: 1
            metadata: 0
```

Wilkins detects that a node with both `pl_prod` and `pl_con` is in a cycle and registers both `setup_passthru_callbacks(vol, "producer")` and `setup_passthru_callbacks(vol, "consumer", pl_con)` automatically.
