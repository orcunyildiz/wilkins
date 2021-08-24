//---------------------------------------------------------------------------
// wilkins top-level interface
//--------------------------------------------------------------------------

#ifndef WILKINS_HPP
#define WILKINS_HPP

#include <dlfcn.h>
#include <map>
#include <string>
#include <vector>

#include <wilkins/dataflow.hpp>

#include <wilkins/types.hpp>
#include <wilkins/comm.hpp>
#include <wilkins/workflow.hpp>

//lowfive headers
#include    "hdf5.h"

#include <diy/mpi/communicator.hpp>

#include    <lowfive/H5VOLProperty.hpp>
#include    <lowfive/vol-dist-metadata.hpp>
namespace l5 = LowFive;

namespace wilkins
{

class Wilkins
{
public:
    Wilkins(CommHandle world_comm,
          Workflow& workflow);
    ~Wilkins();

    //! whether my rank belongs to this workflow node, identified by the name of its func field
    bool my_node(const char* name);

    //! returns the total number of dataflows build by this instance of wilkins
    unsigned int nb_dataflows();

     //orc@12-07
    //! returns a handle for this node's intercomm
    vector<diy::mpi::communicator> intercomms();

    //! returns a handle for this node's producer communicator
    CommHandle prod_comm_handle();
    //! returns a handle for this node's consumer communicator
    CommHandle con_comm_handle();

    //! returns the size of the producers
    int prod_comm_size();
    //! returns the size of the consumers
    int con_comm_size();

    int local_comm_size();              // Return the size of the communicator of the local task
    CommHandle local_comm_handle();     // Return the communicator of the local task
    int local_comm_rank();              // Return the rank of the process within the local rank
    int prod_comm_size(int i);          ///< return the size of the communicator of the producer of the in dataflow i
    int con_comm_size(int i);           ///< return the size of the communicator of the consumer of the out dataflow i

    //! returns the size of the workflow
    int workflow_comm_size();
    //! returns the rank within the workflow
    int workflow_comm_rank();

    Comm* world;

    hid_t plist();

    l5::DistMetadataVOL build_lowfive();

    //orc@14-07: used in lowfive prod for signalling that data is ready
    void commit();

private:
    // builds a vector of dataflows for all links in the workflow
    void build_dataflows(vector<Dataflow*>& dataflows);

    // return index in my_nodes_ of workflow node id
    // -1: not found
    int my_node(int workflow_id);

    // data members
    CommHandle world_comm_;                     // handle to original world communicator
    int workflow_size_;                         // Size of the workflow
    int workflow_rank_;                         // Rank within the workflow
    Workflow workflow_;                         // workflow
    vector<Dataflow*>   dataflows;              // all dataflows for the entire workflow
    vector<Dataflow*>   out_dataflows;          // all my outbound dataflows
    vector<pair<Dataflow*, int>> node_in_dataflows; // all my inbound dataflows in case I am a node, and the corresponding index in the vector dataflows


    int tokens_;                                // Number of empty messages to generate before doing a real get

    //orc@12-07: plist that will be provided to the user code
    hid_t plist_;

    vector<diy::mpi::communicator> intercomms_;                      // intercommunicators
    vector<diy::mpi::communicator> out_intercomms_;                      // out_intercommunicator (prod)
    vector<diy::mpi::communicator> in_intercomms_;                      // in_intercommunicator (con)

};

} // namespace

#endif
