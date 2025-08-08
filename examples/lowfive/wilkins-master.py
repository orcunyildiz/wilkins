# Usage: mpirun -n $nprocs python wilkins-master.py config_file
# config_files= [wilkins_prod_con.yaml, wilkins_prod_2cons.yaml]

import os
from glob import glob
import sys
import argparse
from collections import defaultdict

import pyhenson as h
from mpi4py import MPI
import lowfive
from wilkins.utils import exec_task, import_from, get_passthru_lists
from wilkins import pywilkins as w

def validate_environment():
    # NB: Assuming that user has these env set either i)after spack installation or ii)sourcing vol-plugin.sh explicitly
    # os.environ["HDF5_PLUGIN_PATH"] = "/Users/oyildiz/Work/software/lowfive/build/src"
    # os.environ["HDF5_VOL_CONNECTOR"] = "lowfive under_vol=0;under_info={};"
    if not glob(os.path.join(os.environ["HDF5_PLUGIN_PATH"], "liblowfive.*")):
        raise RuntimeError("Bad HDF5_PLUGIN_PATH, lowfive library not found")

    # needed for torch dataloader module to work on mac https://github.com/pytorch/pytorch/issues/46648
    if sys.platform == "darwin":
        print("Running on macOS")
        import multiprocessing # pylint: disable=C0415

        multiprocessing.set_start_method("fork")

def parse_arguments():
    parser = argparse.ArgumentParser(description="Wilkins master script")
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
    if verbosity == 1:
        lowfive.create_logger("info")
    elif verbosity == 2:
        lowfive.create_logger("debug")

class FlowControl:
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
    validate_environment()


    config_file = sys.argv[1]
    sys.argv = [sys.argv[0]] + sys.argv[2:]
    args = parse_arguments()
    setup_logging(args.verbosity)
    single_iter_passthru = args.passthruSupport == 1

    world = MPI.COMM_WORLD.Dup()
    rank = world.Get_rank()

    # orc@22-11: generating procmap via YAML
    workflow = w.Workflow()
    workflow.make_wflow_from_yaml(config_file)
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
        # bookkeeping of tasks belonging to the execution group
        if rank >= node.start_proc and rank < node.start_proc + node.nprocs:
            my_tasks.append(i)
            passthru_files = node.passthru_files
        i = i + 1

    pm = h.ProcMap(world, procs_yaml)
    nm = h.NameMap()

    a = MPI._addressof(MPI.COMM_WORLD)  # pylint: disable=protected-access
    wilkins = w.Wilkins(a, config_file)

    # orc@31-03: adding for the new control logic: consumer looping until there are files
    wlk_producer = -1
    wlk_consumer = (
        []
    )  # orc@25-07: making this an array for fanin cases as same consumer connected to multiple producer instances
    vol = None
    pl_prod = []
    pl_con = []
    serve_indices = []
    # NB: In some cases, L5 comms should only include subset of processes (e.g., rank 0 from LPS)
    io_proc = wilkins.is_io_proc()
    if io_proc == 1:
        comm = w.get_local_comm(wilkins)
        local_rank = comm.Get_rank()
        intercomms = w.get_intercomms(wilkins)
        vol = lowfive.create_DistMetadataVOL(comm, intercomms)
        l5_props = wilkins.set_lowfive()
        exec_group = []
        set_si = 0

        flow_policies = defaultdict(
            list
        )  # key: prod_name value: (flowPolicy, intercomm index)
        passthru_list = defaultdict(
            list
        )  # key: exec_group value: (prodIndex, conIndex, filename) #orc@09-06: added for passthru support
        flow_exec_group = []
        # support for reading/writing files from/to disk (without matching links)
        for pf in passthru_files:
            vol.set_passthru(pf[0], pf[1])
        for prop in l5_props:
            # print(prop.filename, prop.dset, prop.producer, prop.consumer, prop.execGroup, prop.memory, prop.prodIndex, prop.conIndex, prop.zerocopy, prop.flowPolicy)
            if (
                prop.memory == 1
            ):  # TODO: L5 doesn't support both memory and passthru at the moment. This logic might change once L5 supports both modes.
                vol.set_memory(prop.filename, prop.dset)
            else:
                vol.set_passthru(prop.filename, prop.dset)
                # orc@09-06: constructing the passthru list
                if not passthru_list.get(prop.execGroup):
                    passthru_list[prop.execGroup].append(
                        (prop.prodIndex, prop.conIndex, prop.filename)
                    )
            if prop.consumer == 1 and not any(
                x in prop.execGroup for x in exec_group
            ):  # orc: setting single intercomm per exec_group.
                if ensembles != 1:
                    vol.set_intercomm(prop.filename, prop.dset, prop.conIndex)
                wlk_consumer.append(prop.conIndex)
                exec_group.append(prop.execGroup)
            if (
                prop.producer == 1
            ):  # NB: Task can be both producer and consumer.
                wlk_producer = 1
                if prop.prodIndex not in serve_indices:
                    serve_indices.append(prop.prodIndex)
                if prop.zerocopy == 1:
                    vol.set_zerocopy(prop.filename, prop.dset)
            # adding flow control logic
            if (
                prop.producer == 1
                and prop.flowPolicy != 1
                and not any(x in prop.execGroup for x in flow_exec_group)
            ):  # setting single flow control policy per exec_group
                prod_name = prop.execGroup.split(":")[0]
                flow_exec_group.append(prop.execGroup)
                flow_policies[prod_name].append(
                    (prop.flowPolicy, prop.prodIndex)
                )

        def bsa_cb():
            return serve_indices

        # if any flow control policies, handling them here.
        for fp in flow_policies:
            if wilkins.my_node(fp):
                setup_flow_control(vol, comm, intercomms, serve_indices, flow_policies.get(fp))
                set_si = 1

        if (
            not set_si
        ):  # NB: set serve_indices if not set within the flow control
            vol.set_serve_indices(bsa_cb)

        # orc@09-06: determining the mode.
        pl_prod, pl_con = get_passthru_lists(wilkins, passthru_list)

        # if any cb actions, setting them here.
        for action in actions:
            if wilkins.my_node(action[0]):
                file_name = action[1][0]
                cb_func = action[1][1]
                cb = import_from(file_name, cb_func)
                try:
                    cb(
                        vol, local_rank, pl_con
                    )  # NB: For more advanced callbacks with args, users would need to write their own wilkins.py
                except TypeError:
                    cb(vol, local_rank)

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


if __name__ == "__main__":
    main()
