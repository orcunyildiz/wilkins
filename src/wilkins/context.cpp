#include <wilkins/context.h>

static MPI_Comm* local = 0;
static std::vector<MPI_Comm>* intercomms = 0;

void wilkins_set_intercomms(void* ic)
{
    intercomms = static_cast<std::vector<MPI_Comm>*>(ic);
}

__attribute__ ((visibility ("hidden")))
std::vector<MPI_Comm> wilkins_get_intercomms()
{
    return *intercomms;
}

void wilkins_set_local_comm(void* lc)
{
    local = static_cast<MPI_Comm*>(lc);
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
