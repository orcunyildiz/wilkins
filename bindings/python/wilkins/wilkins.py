# ---------------------------------------------------------------------------
#
# wilkins top-level interface -- Python port of include/wilkins/wilkins.hpp
#                                and src/wilkins/wilkins.cpp
#
# --------------------------------------------------------------------------

from __future__ import annotations

import re
import sys
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

from mpi4py import MPI

from .comm import Comm, comm_size, comm_rank
from .context import wilkins_master, wilkins_get_intercomms, wilkins_get_local_comm
from .dataflow import Dataflow
from .types import WilkinsSizes
from .workflow import Workflow


@dataclass
class LowFiveProperty:
    """Properties for a single LowFive dataset link."""
    filename: str = "*"
    dset: str = "*"
    execGroup: str = ""
    zerocopy: int = 0
    memory: int = 1        # 0: passthru, 1: metadata
    producer: int = 0
    consumer: int = 0
    prodIndex: int = 0     # used for handling multiple flow control policies on the producer
    conIndex: int = 0      # used for setting intercomms on the consumer
    flowPolicy: int = 1


class Wilkins:
    """Top-level Wilkins interface.

    Orchestrates workflow parsing, dataflow creation, communicator building,
    and LowFive property setup.
    """

    def __init__(self, world_comm, config_file: str):
        """Initialize Wilkins.

        Args:
            world_comm: MPI communicator. Can be:
                - An mpi4py Comm object directly, OR
                - An integer address (from ``MPI._addressof(MPI.COMM_WORLD)``)
                  for backwards compatibility with the pybind11 interface.
            config_file: Path to the YAML workflow configuration file.
        """
        # Handle the address-based constructor from the old pybind11 interface.
        # The old pybind11 constructor took an integer from MPI._addressof().
        # In practice this was always MPI.COMM_WORLD. For the pure-Python port,
        # users should pass an mpi4py Comm object directly.
        if isinstance(world_comm, int):
            import warnings
            warnings.warn(
                "Passing an integer MPI address is deprecated. "
                "Pass an mpi4py Comm object directly.",
                DeprecationWarning,
                stacklevel=2,
            )
            world_comm = MPI.COMM_WORLD

        self._world_comm = world_comm
        self.world = Comm(world_comm)

        self._workflow_size = comm_size(world_comm)
        self._workflow_rank = comm_rank(world_comm)

        # Build workflow from YAML
        self._workflow = Workflow()
        if not re.match(r".*yaml$", config_file):
            print(
                "ERROR: Not supported configuration file format. "
                "Please provide the graph definition in YAML.",
                file=sys.stderr,
            )
            sys.exit(1)

        Workflow.make_wflow_from_yaml(self._workflow, config_file)

        # Collect all dataflows
        self._dataflows: List[Dataflow] = []
        self._io_proc = 1
        self._build_dataflows()

        # Inbound dataflows (I am a consumer node)
        self._node_in_dataflows: List[Tuple[Dataflow, int]] = []
        for i in range(len(self._workflow.links)):
            if self._workflow.my_in_link(self._workflow_rank, i):
                self._node_in_dataflows.append((self._dataflows[i], i))

        # Outbound dataflows (I am a producer node)
        self._out_dataflows: List[Dataflow] = []
        for i in range(len(self._workflow.links)):
            if self._workflow.my_out_link(self._workflow_rank, i):
                self._out_dataflows.append(self._dataflows[i])

        self._tokens = 0
        self._filenames: List[str] = []
        self._vec_l5: List[LowFiveProperty] = []
        self._intercomms: List = []
        self._out_intercomms: List = []
        self._in_intercomms: List = []

    def __del__(self):
        # Comm objects handle their own cleanup
        pass

    def my_node(self, name: str) -> bool:
        """Whether my rank belongs to the named workflow node."""
        for i in range(len(self._workflow.nodes)):
            if (self._workflow.my_node(self._workflow_rank, i) and
                    name == self._workflow.nodes[i].func):
                return True
        return False

    def nb_dataflows(self) -> int:
        """Total number of dataflows built by this instance."""
        return len(self._dataflows)

    def _build_dataflows(self):
        """Build a Dataflow object for each link in the workflow."""
        io_proc_ref = [self._io_proc]  # mutable reference emulation
        for i in range(len(self._workflow.links)):
            link = self._workflow.links[i]
            prod = link.prod
            con = link.con

            sizes = WilkinsSizes(
                prod_size=self._workflow.nodes[prod].nprocs,
                prod_writers=self._workflow.nodes[prod].nwriters,
                con_size=self._workflow.nodes[con].nprocs,
                prod_start=self._workflow.nodes[prod].start_proc,
                con_start=self._workflow.nodes[con].start_proc,
            )

            df = Dataflow(
                self._world_comm,
                self._workflow_size,
                self._workflow_rank,
                io_proc_ref,
                sizes,
                prod,
                i,
                con,
                link,
            )
            self._dataflows.append(df)

        self._io_proc = io_proc_ref[0]

    def prod_comm_handle(self):
        """Return the first outbound producer communicator (or world if none)."""
        if self._out_dataflows:
            return self._out_dataflows[0].prod_comm_handle()
        return self._world_comm

    def con_comm_handle(self):
        """Return the first inbound consumer communicator (or world if none)."""
        if self._node_in_dataflows:
            return self._node_in_dataflows[0][0].con_comm_handle()
        return self._world_comm

    def set_lowfive(self) -> List[LowFiveProperty]:
        """Return the computed LowFive properties."""
        return self._vec_l5

    def wait(self):
        """Consumer blocks until producer commits (barrier on passthru intercomms)."""
        if self._node_in_dataflows:
            index = 0
            exec_group_dataflows = []
            for df, _ in self._node_in_dataflows:
                if df.execGroup() not in exec_group_dataflows:
                    exec_group_dataflows.append(df.execGroup())
                    index += 1
                    # Wait for data to be ready for the specific intercomm
                    if df.in_passthru() and not df.in_metadata():
                        self._in_intercomms[index - 1].Barrier()

    def build_intercomms_shared(self, task_name: str) -> List[int]:
        """For shared (TP) mode: return bitmask of which intercomms belong to named task.

        This is the overloaded build_intercomms(string) from C++.
        """
        shared_communicators = []
        shared_dataflows = []

        # I'm a producer
        if self._out_dataflows:
            for df in self._out_dataflows:
                if df.sizes().con_start == df.sizes().prod_start:
                    if df.name() not in shared_dataflows:
                        shared_dataflows.append(df.name())
                        if task_name in df.fullName():
                            shared_communicators.append(1)
                        else:
                            shared_communicators.append(0)

        # I'm a consumer
        if self._node_in_dataflows:
            for df, _ in self._node_in_dataflows:
                if df.sizes().prod_start == df.sizes().con_start:
                    if df.name() not in shared_dataflows:
                        shared_dataflows.append(df.name())
                        if task_name in df.fullName():
                            shared_communicators.append(1)
                        else:
                            shared_communicators.append(0)

        return shared_communicators

    def build_intercomms(self) -> list:
        """Create intercommunicators for all dataflow links.

        Populates vec_l5_, intercomms_, out_intercomms_, in_intercomms_.
        Returns the list of all intercommunicators.
        """
        communicators = []
        out_communicators = []
        in_communicators = []

        local_orig = self.local_comm_handle()
        local = local_orig.Dup()

        exec_group_dataflows = []
        index = 0
        vec_l5 = []

        j = 0  # index into out_dataflows
        k = 0  # index into node_in_dataflows

        for i in range(len(self._workflow.links)):
            if (self._workflow.my_in_link(self._workflow_rank, i) or
                    self._workflow.my_out_link(self._workflow_rank, i)):

                if self._workflow.my_out_link(self._workflow_rank, i):
                    # Outgoing link (I am producer)
                    dflow_name = self._out_dataflows[j].name()
                    # Parse: "filename/dset:consumer_func"
                    full_path = dflow_name.split(":")[0]
                    slash_pos = full_path.find("/")
                    filename = full_path[:slash_pos] if slash_pos != -1 else full_path
                    dset = full_path[slash_pos + 1:] if slash_pos != -1 else ""

                    l5_prop = LowFiveProperty()
                    l5_prop.filename = filename
                    l5_prop.dset = dset
                    l5_prop.execGroup = self._out_dataflows[j].execGroup()
                    l5_prop.memory = 1
                    l5_prop.producer = 1
                    l5_prop.flowPolicy = self._out_dataflows[j].flowPolicy()

                    # Set zerocopy
                    if self._out_dataflows[j].zerocopy():
                        l5_prop.zerocopy = 1

                    # Set passthru
                    if self._out_dataflows[j].out_passthru():
                        l5_prop.memory = 0

                    # Create intercomm (one per execution group)
                    if self._out_dataflows[j].execGroup() not in exec_group_dataflows:
                        exec_group_dataflows.append(self._out_dataflows[j].execGroup())

                        if (self._out_dataflows[j].sizes().con_start ==
                                self._out_dataflows[j].sizes().prod_start):
                            # TP mode (time-partitioned)
                            intercomm = local_orig.Dup()
                        else:
                            # SP mode (space-partitioned)
                            remote_leader = self._out_dataflows[j].sizes().con_start
                            intercomm = local.Create_intercomm(
                                0, self._world_comm, remote_leader, 0
                            )

                        communicators.append(intercomm)
                        out_communicators.append(intercomm)
                        l5_prop.prodIndex = index
                        index += 1
                    else:
                        l5_prop.prodIndex = index - 1

                    j += 1
                    vec_l5.append(l5_prop)

                else:
                    # Incoming link (I am consumer)
                    dflow_name = self._node_in_dataflows[k][0].name()
                    full_path = dflow_name.split(":")[0]
                    slash_pos = full_path.find("/")
                    filename = full_path[:slash_pos] if slash_pos != -1 else full_path
                    dset = full_path[slash_pos + 1:] if slash_pos != -1 else ""

                    l5_prop = LowFiveProperty()
                    l5_prop.filename = filename
                    l5_prop.dset = dset
                    l5_prop.execGroup = self._node_in_dataflows[k][0].execGroup()
                    l5_prop.memory = 1
                    l5_prop.consumer = 1
                    l5_prop.flowPolicy = self._node_in_dataflows[k][0].flowPolicy()

                    # Set passthru/memory at dataset level
                    if self._node_in_dataflows[k][0].in_passthru():
                        l5_prop.memory = 0

                    # Create intercomm (one per execution group)
                    if self._node_in_dataflows[k][0].execGroup() not in exec_group_dataflows:
                        exec_group_dataflows.append(self._node_in_dataflows[k][0].execGroup())

                        if (self._node_in_dataflows[k][0].sizes().prod_start ==
                                self._node_in_dataflows[k][0].sizes().con_start):
                            # TP mode
                            intercomm = local_orig.Dup()
                        else:
                            # SP mode
                            remote_leader = self._node_in_dataflows[k][0].sizes().prod_start
                            intercomm = local.Create_intercomm(
                                0, self._world_comm, remote_leader, 0
                            )

                        communicators.append(intercomm)
                        in_communicators.append(intercomm)
                        l5_prop.conIndex = index
                        index += 1
                    else:
                        l5_prop.conIndex = index - 1

                    k += 1
                    vec_l5.append(l5_prop)

        self._vec_l5 = vec_l5
        self._intercomms = communicators
        self._out_intercomms = out_communicators
        self._in_intercomms = in_communicators

        return communicators

    def filenames(self) -> List[str]:
        """Return filenames for subgraph API."""
        return self._filenames

    def commit(self):
        """Producer signals that data is ready (barrier on passthru out-intercomms)."""
        if not wilkins_master():
            intercomms = self._out_intercomms
        else:
            intercomms = wilkins_get_intercomms()

        exec_group_dataflows = []
        i = 0
        for df in self._out_dataflows:
            if df.execGroup() not in exec_group_dataflows:
                exec_group_dataflows.append(df.execGroup())
                if df.out_passthru() and not df.out_metadata():
                    intercomms[i].Barrier()
                i += 1

    def is_io_proc(self) -> int:
        """Whether this process participates in I/O (i.e., L5 ops)."""
        return self._io_proc

    def prod_comm_size(self, i: int = None) -> int:
        """Return size of producer communicator.

        If *i* is given, returns size for inbound dataflow *i*.
        Otherwise returns size for the first outbound dataflow.
        """
        if i is not None:
            if len(self._node_in_dataflows) > i:
                return self._node_in_dataflows[i][0].sizes().prod_size
            return 0

        if self._out_dataflows:
            return self._out_dataflows[0].sizes().prod_size
        return self._world_comm.Get_size()

    def con_comm_size(self, i: int = None) -> int:
        """Return size of consumer communicator.

        If *i* is given, returns size for outbound dataflow *i*.
        Otherwise returns size for the first inbound dataflow.
        """
        if i is not None:
            if len(self._out_dataflows) > i:
                return self._out_dataflows[i].sizes().con_size
            return 0

        if self._node_in_dataflows:
            return self._node_in_dataflows[0][0].sizes().con_size
        return self._world_comm.Get_rank()  # NOTE: matches C++ bug (uses MPI_Comm_rank)

    def local_comm_size(self) -> int:
        """Return the size of the local task communicator."""
        if self._node_in_dataflows:
            return self._node_in_dataflows[0][0].sizes().con_size
        elif self._out_dataflows:
            return self._out_dataflows[0].sizes().prod_size
        else:
            return self._world_comm.Get_size()

    def local_comm_handle(self):
        """Return the local task MPI communicator."""
        if wilkins_master():
            return wilkins_get_local_comm()

        if self._out_dataflows:
            return self._out_dataflows[0].prod_comm_handle()
        elif self._node_in_dataflows:
            return self._node_in_dataflows[0][0].con_comm_handle()
        else:
            return self._world_comm

    def local_comm_rank(self) -> int:
        """Return rank within the local task communicator."""
        return self.local_comm_handle().Get_rank()

    def workflow_comm_size(self) -> int:
        """Return total workflow size."""
        return self._workflow_size

    def workflow_comm_rank(self) -> int:
        """Return rank within the workflow."""
        return self._workflow_rank


# --------------------------------------------------------------------------
# Module-level helper functions (replacing pybind11 module-level functions)
# --------------------------------------------------------------------------

def get_local_comm(wilkins_obj: Wilkins):
    """Return the local task communicator as an mpi4py communicator.

    Replaces ``pywilkins.get_local_comm(wilkins)``.
    """
    return wilkins_obj.local_comm_handle()


def get_intercomms(wilkins_obj: Wilkins) -> list:
    """Build and return the intercommunicators for this task.

    Replaces ``pywilkins.get_intercomms(wilkins)``.
    """
    return wilkins_obj.build_intercomms()
