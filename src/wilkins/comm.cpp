#include <wilkins/comm.hpp>
#include <cstdio>

CommHandle
wilkins::
Comm::handle()
{
    return handle_;
}

int
wilkins::
Comm::size()
{
    return size_;
}

int
wilkins::
Comm::rank()
{
    return rank_;
}

// forms a communicator from contiguous world ranks of an MPI communicator
// only collective over the ranks in the range [min_rank, max_rank]
wilkins::
Comm::Comm(CommHandle world_comm,
           int min_rank,
           int max_rank,
           int num_srcs,
           int num_dests,
           int start_dest)://,
    min_rank(min_rank),
    num_srcs(num_srcs),
    num_dests(num_dests),
    start_dest(start_dest)
{
    MPI_Group group, newgroup;
    int range[3];
    range[0] = min_rank;
    range[1] = max_rank;
    range[2] = 1;
    MPI_Comm_group(world_comm, &group);
    MPI_Group_range_incl(group, 1, &range, &newgroup);
    MPI_Comm_create_group(world_comm, newgroup, 0, &handle_);
    MPI_Group_free(&group);
    MPI_Group_free(&newgroup);

    MPI_Comm_rank(handle_, &rank_);
    MPI_Comm_size(handle_, &size_);
    new_comm_handle_ = true;
}

// wraps a wilkins communicator around an entire MPI communicator
wilkins::
Comm::Comm(CommHandle world_comm):
    handle_(world_comm),
    min_rank(0)
{
    num_srcs         = 0;
    num_dests        = 0;
    start_dest       = 0;
    new_comm_handle_ = false;

    MPI_Comm_rank(handle_, &rank_);
    MPI_Comm_size(handle_, &size_);
}

wilkins::
Comm::~Comm()
{
    if (new_comm_handle_)
        MPI_Comm_free(&handle_);
}

int
wilkins::CommRank(CommHandle comm)
{
    int rank;
    MPI_Comm_rank(comm, &rank);
    return rank;
}

int
wilkins::CommSize(CommHandle comm)
{
    int size;
    MPI_Comm_size(comm, &size);
    return size;
}

Address
wilkins::addressof(const void *addr)
{
    MPI_Aint p;
    MPI_Get_address(addr, &p);
    return p;
}
