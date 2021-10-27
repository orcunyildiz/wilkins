#include <wilkins/context.h>
#include <diy/mpi/communicator.hpp>

static diy::mpi::communicator* local = 0;
static std::vector<diy::mpi::communicator>* intercomms = 0;

void wilkins_set_intercomms(void* ic)
{
    intercomms = static_cast<std::vector<diy::mpi::communicator>*>(ic);
}

__attribute__ ((visibility ("hidden")))
std::vector<diy::mpi::communicator> wilkins_get_intercomms()
{
    return *intercomms;
}

void wilkins_set_local_comm(void* lc)
{
    local = static_cast<diy::mpi::communicator*>(lc);
}

__attribute__ ((visibility ("hidden")))
MPI_Comm wilkins_get_local_comm()
{
    return *local;
}

__attribute__ ((visibility ("hidden")))
int         wilkins_master()
{
    return  intercomms!= 0;
}
