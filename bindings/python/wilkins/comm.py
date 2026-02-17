# ---------------------------------------------------------------------------
#
# wilkins communicator interface -- Python port of include/wilkins/comm.hpp
#                                   and src/wilkins/comm.cpp
#
# --------------------------------------------------------------------------

from mpi4py import MPI


class Comm:
    """Generic communication mechanism for producer, consumer, dataflow.

    Ranks in communicator are contiguous in the world.
    """

    def __init__(self, world_comm, min_rank=None, max_rank=None,
                 num_srcs=0, num_dests=0, start_dest=0):
        """Create a communicator.

        Two modes:
          1. Range mode: ``Comm(world_comm, min_rank, max_rank, ...)``
             Creates a sub-communicator from contiguous world ranks
             [min_rank, max_rank]. Only collective over those ranks.
          2. Wrap mode: ``Comm(world_comm)``
             Wraps an existing MPI communicator without creating a new one.
        """
        self._num_srcs = num_srcs
        self._num_dests = num_dests
        self._start_dest = start_dest
        self._new_comm_handle = False

        if min_rank is not None and max_rank is not None:
            # Range mode: create sub-communicator from contiguous ranks
            self._min_rank = min_rank
            group = world_comm.Get_group()
            new_group = group.Range_incl([(min_rank, max_rank, 1)])
            self._handle = world_comm.Create_group(new_group, tag=0)
            group.Free()
            new_group.Free()
            self._rank = self._handle.Get_rank()
            self._size = self._handle.Get_size()
            self._new_comm_handle = True
        else:
            # Wrap mode: wrap an entire MPI communicator
            self._handle = world_comm
            self._min_rank = 0
            self._rank = self._handle.Get_rank()
            self._size = self._handle.Get_size()

    def __del__(self):
        if self._new_comm_handle and self._handle != MPI.COMM_NULL:
            self._handle.Free()

    def handle(self):
        """Return the underlying MPI communicator."""
        return self._handle

    def size(self):
        """Return communicator size."""
        return self._size

    def rank(self):
        """Return rank in communicator."""
        return self._rank


def comm_rank(comm):
    """Return rank in the given MPI communicator."""
    return comm.Get_rank()


def comm_size(comm):
    """Return size of the given MPI communicator."""
    return comm.Get_size()
