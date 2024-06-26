#ifndef WILKINS_CONTEXT_H
#define WILKINS_CONTEXT_H

#include <mpi.h>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

int         wilkins_master();

void        wilkins_set_intercomms(void* local);
std::vector<MPI_Comm>    wilkins_get_intercomms();

void        wilkins_set_local_comm(void* local);
MPI_Comm    wilkins_get_local_comm();

#ifdef __cplusplus
}
#endif

#endif
