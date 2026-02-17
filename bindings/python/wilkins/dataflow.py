# ---------------------------------------------------------------------------
#
# dataflow interface -- Python port of include/wilkins/dataflow.hpp
#                       and src/wilkins/dataflow.cpp
#
# --------------------------------------------------------------------------

from __future__ import annotations

import sys

from mpi4py import MPI

from .types import (
    WILKINS_OTHER_COMM,
    WILKINS_PRODUCER_COMM,
    WILKINS_CONSUMER_COMM,
    WilkinsSizes,
)
from .comm import Comm
from .workflow import WorkflowLink
from .context import wilkins_master


class Dataflow:
    """Represents a single producer-consumer dataflow link.

    World is partitioned into {producer, dataflow, consumer, other} in
    increasing rank.
    """

    def __init__(
        self,
        world_comm,          # MPI communicator (mpi4py Comm)
        workflow_size: int,  # size of the workflow
        workflow_rank: int,  # rank in the workflow
        io_proc: list,       # mutable list [io_proc_value] -- pass-by-ref emulation
        wilkins_sizes: WilkinsSizes,
        prod: int,           # id in workflow structure of producer node
        dflow: int,          # id in workflow structure of dataflow link
        con: int,            # id in workflow structure of consumer node
        wflow_link: WorkflowLink,
    ):
        self._world_comm = world_comm
        self._world_size = workflow_size
        self._world_rank = workflow_rank
        self._sizes = WilkinsSizes(
            prod_size=wilkins_sizes.prod_size,
            prod_writers=wilkins_sizes.prod_writers,
            con_size=wilkins_sizes.con_size,
            prod_start=wilkins_sizes.prod_start,
            con_start=wilkins_sizes.con_start,
        )
        self._wflow_prod_id = prod
        self._wflow_con_id = con
        self._wflow_dflow_id = dflow
        self._type = WILKINS_OTHER_COMM
        self._tokens = 0

        # LowFive related flags
        self._in_passthru = wflow_link.in_passthru
        self._in_metadata = wflow_link.in_metadata
        self._out_passthru = wflow_link.out_passthru
        self._out_metadata = wflow_link.out_metadata
        self._zerocopy = wflow_link.zerocopy
        self._flow_policy = wflow_link.flow_policy

        self._name = wflow_link.name
        self._full_name = wflow_link.fullName
        self._exec_group = wflow_link.execGroup

        self._prod_comm = None
        self._con_comm = None

        # Ensure sizes and starts fit in the world
        if (self._sizes.prod_start + self._sizes.prod_size > self._world_size or
                self._sizes.con_start + self._sizes.con_size > self._world_size):
            print(
                "Wilkins error: Group sizes of producer, consumer, and dataflow "
                "exceed total size of world communicator",
                file=sys.stderr,
            )
            return

        # Communicator creation -- only applies to MPMD mode for the user codes
        if not wilkins_master():
            # Producer
            if (self._world_rank >= self._sizes.prod_start and
                    self._world_rank < self._sizes.prod_start + self._sizes.prod_size):
                self._type |= WILKINS_PRODUCER_COMM

                # Supporting subset of writers for prod
                if self._sizes.prod_writers == -1:
                    self._prod_comm = Comm(
                        world_comm,
                        self._sizes.prod_start,
                        self._sizes.prod_start + self._sizes.prod_size - 1,
                    )
                elif self._world_rank < self._sizes.prod_start + self._sizes.prod_writers:
                    self._prod_comm = Comm(
                        world_comm,
                        self._sizes.prod_start,
                        self._sizes.prod_start + self._sizes.prod_writers - 1,
                    )
                else:
                    # Used by wilkins.py to determine which procs should join L5 ops
                    io_proc[0] = 0

            # Consumer
            # Last condition prevents duplicate comms in TP mode
            if (self._world_rank >= self._sizes.con_start and
                    self._world_rank < self._sizes.con_start + self._sizes.con_size and
                    self._sizes.con_start != self._sizes.prod_start):
                self._type |= WILKINS_CONSUMER_COMM
                self._con_comm = Comm(
                    world_comm,
                    self._sizes.con_start,
                    self._sizes.con_start + self._sizes.con_size - 1,
                )
                self._tokens = wflow_link.tokens

    def __del__(self):
        # In MPMD mode, free created communicators
        if not wilkins_master():
            # Comm objects handle their own cleanup via __del__
            pass

    def sizes(self) -> WilkinsSizes:
        """Return the sizes struct."""
        return self._sizes

    def is_prod(self) -> bool:
        """Whether this rank is a producer."""
        return (self._type & WILKINS_PRODUCER_COMM) == WILKINS_PRODUCER_COMM

    def is_con(self) -> bool:
        """Whether this rank is a consumer."""
        return (self._type & WILKINS_CONSUMER_COMM) == WILKINS_CONSUMER_COMM

    def is_prod_root(self) -> bool:
        """Whether this rank is the producer root."""
        return self._world_rank == self._sizes.prod_start

    def is_con_root(self) -> bool:
        """Whether this rank is the consumer root."""
        return self._world_rank == self._sizes.con_start

    def prod_comm_handle(self):
        """Return the producer MPI communicator."""
        return self._prod_comm.handle()

    def con_comm_handle(self):
        """Return the consumer MPI communicator."""
        return self._con_comm.handle()

    def in_passthru(self) -> int:
        """Consumer passthru flag."""
        return self._in_passthru

    def in_metadata(self) -> int:
        """Consumer metadata flag."""
        return self._in_metadata

    def out_passthru(self) -> int:
        """Producer passthru flag."""
        return self._out_passthru

    def out_metadata(self) -> int:
        """Producer metadata flag."""
        return self._out_metadata

    def flowPolicy(self) -> int:
        """Flow control policy."""
        return self._flow_policy

    def zerocopy(self) -> int:
        """Zerocopy flag."""
        return self._zerocopy

    def name(self) -> str:
        """Link name."""
        return self._name

    def execGroup(self) -> str:
        """Execution group name."""
        return self._exec_group

    def fullName(self) -> str:
        """Full link name with source."""
        return self._full_name
