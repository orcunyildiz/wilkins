# ---------------------------------------------------------------------------
# Wilkins workflow driver entry point.
#
# This module provides the ``main()`` function used by the
# ``wilkins-master`` console script (installed via pip).
#
# Usage:
#   mpirun -n $nprocs wilkins-master config.yaml [-p 0|1] [-v 0|1|2]
#
# Or equivalently:
#   mpirun -n $nprocs python -m wilkins.master config.yaml [-p 0|1] [-v 0|1|2]
# ---------------------------------------------------------------------------

import os
from glob import glob
import sys
import argparse
from collections import defaultdict

try:
    import pyhenson as h
    HAS_HENSON = True
except ImportError:
    HAS_HENSON = False

from mpi4py import MPI

try:
    import lowfive
    HAS_LOWFIVE = True
except ImportError:
    HAS_LOWFIVE = False

from .utils import exec_task, import_from, get_passthru_lists, setup_passthru_callbacks
from .workflow import Workflow
from .wilkins import Wilkins, get_local_comm, get_intercomms


def validate_environment():
    """Verify the HDF5 plugin path contains the LowFive library."""
    plugin_path = os.environ.get("HDF5_PLUGIN_PATH", "")
    if not plugin_path or not glob(os.path.join(plugin_path, "liblowfive.*")):
        raise RuntimeError(
            "Bad or missing HDF5_PLUGIN_PATH: lowfive library not found. "
            "Set HDF5_PLUGIN_PATH to the directory containing liblowfive."
        )

    # Needed for torch dataloader module to work on macOS
    # https://github.com/pytorch/pytorch/issues/46648
    if sys.platform == "darwin":
        print("Running on macOS")
        import multiprocessing
        multiprocessing.set_start_method("fork")


def parse_arguments():
    parser = argparse.ArgumentParser(description="Wilkins workflow driver")
    parser.add_argument(
        "-p",
        "--passthruSupport",
        type=int,
        choices=[0, 1],
        default=0,
        help=(
            "Passthru support level for tasks (0: none [default], 1: single"
            " iteration)"
        ),
    )
    parser.add_argument(
        "-v",
        "--verbosity",
        type=int,
        choices=[0, 1, 2],
        default=0,
        help="Adjust logging level (0: none [default], 1: info, 2: debug)",
    )

    return parser.parse_args()


def setup_logging(verbosity):
    if not HAS_LOWFIVE:
        return
    if verbosity == 1:
        lowfive.create_logger("info")
    elif verbosity == 2:
        lowfive.create_logger("debug")


class FlowControl:
    """Manages per-link I/O frequency policies for flow control."""

    def __init__(self, vol, comm, intercomms, serve_indices, flow_policies):
        self.comm = comm
        self.intercomms = intercomms
        self.serve_indices = serve_indices
        self.flow_policies = flow_policies
        self.serve_counter = 0
        self.vol = vol

    def callback(self):
        self.comm.barrier()
        self.serve_counter += 1
        indices = list(range(len(self.serve_indices)))
        serve = 0

        for interval, idx in self.flow_policies:
            if interval == -1:
                if self.intercomms[idx].iprobe(tag=2):
                    serve = 1
                if self.comm.allreduce(serve, op=MPI.MAX):
                    pass  # serving
                else:
                    indices.remove(idx)
            elif self.serve_counter % interval != 0:
                indices.remove(idx)

        return indices


def setup_flow_control(vol, comm, intercomms, serve_indices, flow_policies):
    fc = FlowControl(vol, comm, intercomms, serve_indices, flow_policies)
    vol.set_serve_indices(fc.callback)


def main():
    """Main entry point for the Wilkins workflow driver."""
    validate_environment()

    if not HAS_LOWFIVE:
        raise RuntimeError(
            "The 'lowfive' Python package is required to run the Wilkins driver. "
            "Install it via Spack or from source."
        )

    config_file = sys.argv[1]
    sys.argv = [sys.argv[0]] + sys.argv[2:]
    args = parse_arguments()
    setup_logging(args.verbosity)
    single_iter_passthru = args.passthruSupport == 1

    world = MPI.COMM_WORLD.Dup()
    rank = world.Get_rank()

    # Generate procmap via YAML
    workflow = Workflow()
    Workflow.make_wflow_from_yaml(workflow, config_file)
    procs_yaml = []
    my_tasks = []
    puppets = []
    actions = []
    passthru_files = []
    i = 0
    ensembles = 0
    for node in workflow.nodes:
        procs_yaml.append((node.func, node.nprocs))
        if node.taskCount > 1:
            ensembles = 1
        if not ensembles:
            task_exec = node.func
        else:
            task_exec = "./" + node.func + ".hx"
        puppets.append((task_exec, node.args))
        if node.actions:
            actions.append((node.func, node.actions))
        # Bookkeeping of tasks belonging to the execution group
        if rank >= node.start_proc and rank < node.start_proc + node.nprocs:
            my_tasks.append(i)
            passthru_files = node.passthru_files
        i = i + 1

    if not HAS_HENSON:
        raise RuntimeError(
            "The 'pyhenson' package is required to run the Wilkins driver. "
            "Install Henson and its Python bindings via Spack or from source."
        )

    pm = h.ProcMap(world, procs_yaml)
    nm = h.NameMap()

    wilkins = Wilkins(MPI.COMM_WORLD, config_file)

    # Consumer looping until there are files
    wlk_producer = -1
    wlk_consumer = []
    vol = None
    pl_prod = []
    pl_con = []
    serve_indices = []
    # In some cases, L5 comms should only include subset of processes
    io_proc = wilkins.is_io_proc()
    if io_proc == 1:
        comm = get_local_comm(wilkins)
        local_rank = comm.Get_rank()
        intercomms = get_intercomms(wilkins)
        vol = lowfive.create_DistMetadataVOL(comm, intercomms)
        l5_props = wilkins.set_lowfive()
        exec_group = []
        set_si = 0

        flow_policies = defaultdict(list)
        passthru_list = defaultdict(list)
        flow_exec_group = []

        # Support for reading/writing files from/to disk (without matching links)
        for pf in passthru_files:
            vol.set_passthru(pf[0], pf[1])

        for prop in l5_props:
            if prop.memory == 1:
                vol.set_memory(prop.filename, prop.dset)
            else:
                vol.set_passthru(prop.filename, prop.dset)
                if not passthru_list.get(prop.execGroup):
                    passthru_list[prop.execGroup].append(
                        (prop.prodIndex, prop.conIndex, prop.filename)
                    )

            if prop.consumer == 1 and not any(
                x in prop.execGroup for x in exec_group
            ):
                if ensembles != 1:
                    vol.set_intercomm(prop.filename, prop.dset, prop.conIndex)
                wlk_consumer.append(prop.conIndex)
                exec_group.append(prop.execGroup)

            if prop.producer == 1:
                wlk_producer = 1
                if prop.prodIndex not in serve_indices:
                    serve_indices.append(prop.prodIndex)
                if prop.zerocopy == 1:
                    vol.set_zerocopy(prop.filename, prop.dset)

            # Flow control logic
            if (
                prop.producer == 1
                and prop.flowPolicy != 1
                and not any(x in prop.execGroup for x in flow_exec_group)
            ):
                prod_name = prop.execGroup.split(":")[0]
                flow_exec_group.append(prop.execGroup)
                flow_policies[prod_name].append(
                    (prop.flowPolicy, prop.prodIndex)
                )

        def bsa_cb():
            return serve_indices

        # If any flow control policies, handle them here
        for fp in flow_policies:
            if wilkins.my_node(fp):
                setup_flow_control(
                    vol, comm, intercomms, serve_indices, flow_policies.get(fp)
                )
                set_si = 1

        if not set_si:
            vol.set_serve_indices(bsa_cb)

        # Determine passthru mode
        pl_prod, pl_con = get_passthru_lists(wilkins, passthru_list)

        # If any callback actions, set them here
        for action in actions:
            if wilkins.my_node(action[0]):
                file_name = action[1][0]
                cb_func = action[1][1]
                cb = import_from(file_name, cb_func)
                try:
                    cb(vol, local_rank, pl_con)
                except TypeError:
                    cb(vol, local_rank)

        # For cycle topologies: a node that is both producer and consumer in
        # passthru mode needs *both* producer and consumer callbacks.  The YAML
        # actions mechanism only registers one side (e.g. ``prod_callback`` for
        # the cycle-entry node, ``con_callback`` for the others).
        # Unconditionally register both so that every node in the cycle can
        # (a) signal downstream consumers after writing (producer callback) and
        # (b) block until the upstream producer signals data is ready (consumer
        # callback).  Re-registering an already-set callback is harmless — the
        # generic passthru callbacks are idempotent.
        if pl_prod and pl_con:
            setup_passthru_callbacks(vol, "producer")
            setup_passthru_callbacks(vol, "consumer", pl_con)

    exec_task(
        wilkins,
        puppets,
        my_tasks,
        vol,
        wlk_consumer,
        wlk_producer,
        pl_prod,
        pl_con,
        pm,
        nm,
        io_proc,
        ensembles,
        serve_indices,
        single_iter_passthru,
    )

    # Ensure all ranks (including non-writer ranks that finish early)
    # stay alive until the entire workflow completes.  Without this,
    # early-exiting ranks trigger Python/HDF5 teardown (H5_term_library)
    # while other ranks are still actively using LowFive, which kills
    # the whole MPI job.
    MPI.COMM_WORLD.Barrier()


if __name__ == "__main__":
    main()
