//---------------------------------------------------------------------------
// dataflow interface
//
// Tom Peterka
// Argonne National Laboratory
// 9700 S. Cass Ave.
// Argonne, IL 60439
// tpeterka@mcs.anl.gov
//
//--------------------------------------------------------------------------

#ifndef WILKINS_DATAFLOW_HPP
#define WILKINS_DATAFLOW_HPP

#include <map>
#include <string>

#include <wilkins/comm.hpp>
#include <wilkins/types.hpp>

#include <wilkins/workflow.hpp>
#include <wilkins/context.h>

#include <memory>
#include <queue>

namespace wilkins
{
// world is partitioned into {producer, dataflow, consumer, other} in increasing rank
class Dataflow
{
public:
    Dataflow(CommHandle world_comm,             ///<  world communicator
             int workflow_size,                 ///<  size of the workflow
             int workflow_rank,                 ///<  rank in the workflow
             WilkinsSizes& wilkins_sizes,           ///<  sizes of producer, dataflow, consumer
             int prod,                          ///<  id in workflow structure of producer node
             int dflow,                         ///<  id in workflow structure of dataflow link
             int con,                           ///<  id in workflow structure of consumer node
             WorkflowLink wflowLink);

    ~Dataflow();

    WilkinsSizes* sizes();

    // whether this rank is producer or consumer
    bool is_prod();
    bool is_con();
    bool is_prod_root();
    bool is_con_root();
    CommHandle prod_comm_handle();
    CommHandle con_comm_handle();

    //orc@12-07: lowfive stuff
    int in_passthru();
    int in_metadata();
    int out_passthru();
    int out_metadata();

    int zerocopy();
    string name();
    string execGroup();
    string fullName();

private:
    CommHandle world_comm_;          // handle to original world communicator
    Comm* prod_comm_;                // producer communicator
    Comm* con_comm_;                 // consumer communicator
    int world_rank_;
    int world_size_;

    WilkinsSizes sizes_;               // sizes of communicators, time steps
    CommTypeWilkins type_;             // whether this instance is producer, consumer,
    // dataflow, or other
    int wflow_prod_id_;              // index of corresponding producer in the workflow
    int wflow_con_id_;               // index of corresponding consumer in the workflow
    int wflow_dflow_id_;             // index of corresponding link in the workflow

    int tokens_;                     // Number of empty message to receive on destPort_
    int in_passthru_;                   // lowfive-con: write to file
    int in_metadata_;                   // lowfive-con: build and use in-memory metadata
    int out_passthru_;                   // lowfive-prod: write to file
    int out_metadata_;                   // lowfive-prod: build and use in-memory metadata
    int zerocopy_;                  // lowfive: set zerocopy of dataset (default (0) is lowfive (deep copy), 1 means shallow copy)

    string name_;                    //name of the link, used in enforcing zerocopy (prod) or setting intercomm for dsets (con)
    string fullName_;                //name of the link with the source/producer of the link, used for differentiating links where the rank info is not sufficient (shared mode)
    string execGroup_;                //name of the execution group used to prevent multiple intercomm creation within this execution group.

};// End of class Dataflow

} // namespace



#endif
