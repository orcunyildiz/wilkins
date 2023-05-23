//---------------------------------------------------------------------------
// wilkins top-level interface
//--------------------------------------------------------------------------

#ifndef WILKINS_HPP
#define WILKINS_HPP

#include <dlfcn.h>
#include <map>
#include <string>
#include <regex>
#include <vector>
#include <algorithm>

#include <wilkins/dataflow.hpp>

#include <wilkins/types.hpp>
#include <wilkins/comm.hpp>
#include <wilkins/workflow.hpp>
#include <wilkins/context.h>

//orc: We would need to separate whether we are producer or consumer and depending on that we are calling set zerocopy or set intercomm at wilkins.py
struct LowFiveProperty
{
    string filename="*";
    string dset="*";
    string execGroup="";
    int zerocopy=0;
    int memory=1; //0:passthru 1:metadata
    int producer=0;
    int consumer=0;
    int prodIndex=0; // used for handling multiple flow control policies on the producer
    int conIndex=0; // used for setting intercomms on the consumer
    int flowPolicy=1;
};

namespace wilkins
{

class Wilkins
{
public:
    Wilkins(CommHandle world_comm,
          const string& config_file);
    ~Wilkins();

    //! whether my rank belongs to this workflow node, identified by the name of its func field
    bool my_node(const char* name);

    //! returns the total number of dataflows build by this instance of wilkins
    unsigned int nb_dataflows();

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

    vector<std::string> filenames();
    int is_io_proc();

    vector<LowFiveProperty> set_lowfive();
    void wait(); //orc@06-10: consumer waits until producer issues a commit function.
    vector<MPI_Comm> build_intercomms();
    vector<int> build_intercomms(std::string task_name); //orc@05-11: used for shared mode
    //orc@14-07: used in lowfive prod for signalling that data is ready
    void commit();

private:
    // builds a vector of dataflows for all links in the workflow
    void build_dataflows(vector<Dataflow*>& dataflows);

    // data members
    CommHandle world_comm_;                     // handle to original world communicator
    int workflow_size_;                         // Size of the workflow
    int workflow_rank_;                         // Rank within the workflow
    Workflow workflow_;                         // workflow
    vector<Dataflow*>   dataflows;              // all dataflows for the entire workflow
    vector<Dataflow*>   out_dataflows;          // all my outbound dataflows
    vector<pair<Dataflow*, int>> node_in_dataflows; // all my inbound dataflows in case I am a node, and the corresponding index in the vector dataflows


    int tokens_;                                // Number of empty messages to generate before doing a real get
    int io_proc_;				// indicates whether process participates in I/O (i.e., L5 ops)

    //wilkins provides filenames to the user tasks for subgraph API
    vector<std::string> filenames_;

    vector<LowFiveProperty> vec_l5_;                       // l5 properties

    //orc@27-10: deprecated as they are used in build_lowfive(), which is deprecated as well.
    //NB: out_intercomms_ is used also in commit().
    //TODO: omit them once commit() is ready to be updated, which is waiting on the L5 design finalization.
    vector<MPI_Comm> intercomms_;                          // intercommunicators
    vector<MPI_Comm> out_intercomms_;                      // out_intercommunicator (prod)
    vector<MPI_Comm> in_intercomms_;                       // in_intercommunicator (con)

};

} // namespace

#endif
