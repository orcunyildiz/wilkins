//---------------------------------------------------------------------------
//
// wilkins communicator interface
//
//--------------------------------------------------------------------------
#ifndef WILKINS_COMM_H
#define WILKINS_COMM_H

#include <wilkins/types.hpp>
#include <vector>
#include <math.h>

namespace wilkins
{

    // generic communication mechanism for producer, consumer, dataflow
    // ranks in communicator are contiguous in the world
    class Comm
    {
    public:
        Comm(CommHandle world_comm,
             int min_rank,
             int max_rank,
             int num_srcs = 0,
             int num_dests = 0,
             int start_dest = 0);//,
        Comm(CommHandle world_comm);
        ~Comm();
        CommHandle handle();
        int size();
        int rank();

    private:
        CommHandle handle_;                  // communicator handle in the transport layer
        int size_;                           // communicator size
        int rank_;                           // rank in communicator
        int min_rank;                        // min (world) rank of communicator
        int num_srcs;             // number of sources (producers) within the communicator
        int num_dests;            // numbers of destinations (consumers) within the communicator
        int start_dest;           // first destination rank within the communicator (0 to size_ - 1)
        bool new_comm_handle_;    // a new low level communictor (handle) was created
    };

    int CommRank(CommHandle comm);
    int CommSize(CommHandle comm);
    Address addressof(const void *addr);

} // namespace

#endif
