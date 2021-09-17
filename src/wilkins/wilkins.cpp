#include <wilkins/wilkins.hpp>

// constructor
wilkins::
Wilkins::Wilkins(CommHandle world_comm,
	     const string& config_file) :
    world_comm_(world_comm)
{
    world = new Comm(world_comm);

    workflow_size_ = CommSize(world_comm);
    workflow_rank_ = CommRank(world_comm);

    //build workflow
    //omitting json support now, can add later if needed.
    //std::regex jsn ("(.*json)");
    std::regex yml ("(.*yaml)");
    if (std::regex_match (config_file,yml))
        Workflow::make_wflow_from_yaml(workflow_, config_file);
    //else if (std::regex_match (config_file,jsn))
    //    Workflow::make_wflow_from_json(workflow_, config_file);
    else{
        fprintf(stderr, "ERROR: Not supported configuration file format. Please provide the graph definition in YAML.\n");
        exit(1);
    }

    // collect all dataflows
    build_dataflows(dataflows);

    // inbound dataflows
    for (size_t i = 0; i < workflow_.links.size(); i++)
    {

        // I am a node and this dataflow is an input
        if (workflow_.my_in_link(workflow_rank_, i))
        {
            node_in_dataflows.push_back(pair<Dataflow*, int>(dataflows[i], i));
        }
    }

     // outbound dataflows
    for (size_t i = 0; i < workflow_.links.size(); i++)
    {
        // I am a node and this dataflow is an output
        if (workflow_.my_out_link(workflow_rank_, i))
        {

            out_dataflows.push_back(dataflows[i]);
        }
    }

}

// destructor
wilkins::
Wilkins::~Wilkins()
{
    delete world;
}


// whether my rank belongs to this workflow node, identified by the name of its func field
bool
wilkins::
Wilkins::my_node(const char* name)
{
    for (size_t i = 0; i < workflow_.nodes.size(); i++)
    {
        if (workflow_.my_node(workflow_rank_, i) &&
                !strcmp(name, workflow_.nodes[i].func.c_str()))
            return true;
    }
    return false;
}

// Return the total number of dataflows build by this instance of wilkins
unsigned int
wilkins::
Wilkins::nb_dataflows()
{
    return dataflows.size();
}

// builds a vector of dataflows for all links in the workflow
void
wilkins::
Wilkins::build_dataflows(vector<Dataflow*>& dataflows)
{
    WilkinsSizes wilkins_sizes;
    for (size_t i = 0; i < workflow_.links.size(); i++)
    {
        int prod  = workflow_.links[i].prod;    // index into workflow nodes
        int dflow = i;                          // index into workflow links
        int con   = workflow_.links[i].con;     // index into workflow nodes
        wilkins_sizes.prod_size           = workflow_.nodes[prod].nprocs;
        wilkins_sizes.con_size            = workflow_.nodes[con].nprocs;
        wilkins_sizes.prod_start          = workflow_.nodes[prod].start_proc;
        wilkins_sizes.con_start           = workflow_.nodes[con].start_proc;

        dataflows.push_back(new Dataflow(world_comm_,
                                         workflow_size_,
                                         workflow_rank_,
                                         wilkins_sizes,
                                         prod,
                                         dflow,
                                         con,
                                         workflow_.links[i]));

    }
}

std::vector<diy::mpi::communicator>
wilkins::
Wilkins::intercomms()
{
        return intercomms_;
}

CommHandle
wilkins::
Wilkins::prod_comm_handle()
{
    if (!out_dataflows.empty())
        return out_dataflows[0]->prod_comm_handle();
    else
        return world_comm_; // The task is the only one in the graph
}

CommHandle
wilkins::
Wilkins::con_comm_handle()
{
    if (!node_in_dataflows.empty())
        return node_in_dataflows[0].first->con_comm_handle();
    else
        return world_comm_; // The task is the only one in the graph
}



l5::DistMetadataVOL
wilkins::
Wilkins::build_lowfive()
{
    //orc@13-07: setting this only for the lowfive object creation, but should be defined for each prod-con pair as we did below (df->sizes()->con_start == df->sizes()->prod_start)
    bool shared = false;
    int passthru = 0; //orc@13-07: this should be also for each prod-con pair
    int metadata = 1; //orc@13-07: this should be also for each prod-con pair
    int ownership = 0;
    std::vector<diy::mpi::communicator> communicators, out_communicators, in_communicators;
    diy::mpi::communicator intercomm, local;
    local = diy::mpi::communicator(this->local_comm_handle());
    std::string dflowName, full_path, filename, dset;

    //I'm a producer
    if (!out_dataflows.empty())
    {

        for (Dataflow* df : out_dataflows)
        {
	    passthru = df->out_passthru();
            metadata = df->out_metadata();
            ownership = df->ownership();

            //TODO: first set of string ops are only for debugging, delete later.
            dflowName = df->name();
            stringstream line(dflowName);
            std::getline(line, full_path, ':');
            stringstream line2(full_path);
            std::getline(line2, filename, '/');
            dset = full_path.substr(full_path.find("/") + 1);

            if (df->sizes()->con_start == df->sizes()->prod_start)
                intercomm   = local;
            else
            {
                MPI_Comm intercomm_;
	        int remote_leader = df->sizes()->con_start;
                MPI_Intercomm_create(local, 0, world_comm_, remote_leader,  0, &intercomm_);
                intercomm = diy::mpi::communicator(intercomm_);
            }

            fmt::print("local.size() = {}, intercomm.size() = {}, passthru = {}, metadata = {}, ownership = {}, filename = {}, dset = {}\n", local.size(), intercomm.size(), passthru, metadata, ownership, filename, dset);

            communicators.push_back(intercomm);
            out_communicators.push_back(intercomm);

        }

    }

    //I'm a consumer
    if (!node_in_dataflows.empty())
    {

        for (std::pair<Dataflow*, int> pair : node_in_dataflows)
        {

            passthru = pair.first->in_passthru();
            metadata = pair.first->in_metadata();
            //TODO: resolve conflicting passthru/metadata flags for prod-con pairs
            //int out_passthru = pair.first->out_passthru();
            //int out_metadata = pair.first->out_metadata();
	    //if(out_passthru && !out_metadata)
	    //	passthru = out_passthru;

            //TODO: first set of string ops are only for debugging, delete later.
            dflowName = pair.first->name();
            stringstream line(dflowName);
            std::getline(line, full_path, ':');
            stringstream line2(full_path);
            std::getline(line2, filename, '/');
            dset = full_path.substr(full_path.find("/") + 1);

            if (pair.first->sizes()->prod_start == pair.first->sizes()->con_start)
                intercomm   = local;
            else
            {
                MPI_Comm intercomm_;
                int remote_leader = pair.first->sizes()->prod_start;
     	        MPI_Intercomm_create(local, 0, world_comm_, remote_leader,  0, &intercomm_);
                intercomm = diy::mpi::communicator(intercomm_);
            }

            fmt::print("local.size() = {}, intercomm.size() = {}, passthru = {}, metadata = {}, filename = {}, dset = {}\n", local.size(), intercomm.size(), passthru, metadata, filename, dset);

            communicators.push_back(intercomm);
            in_communicators.push_back(intercomm);

        }

    }

    // set up file access property list
    this->plist_ =  H5Pcreate(H5P_FILE_ACCESS);
    if (passthru)
        H5Pset_fapl_mpio(plist_, local, MPI_INFO_NULL);

    // set up lowfive
    l5::DistMetadataVOL vol_plugin(local, communicators, metadata, passthru);
    l5::H5VOLProperty vol_prop(vol_plugin);
    vol_prop.apply(plist_);

    //prod-con related vol_plugin ops: setting ownership and intercomm per dataset
    //I'm a producer
    if (!out_dataflows.empty())
    {
     	for (Dataflow* df : out_dataflows)
        {

            dflowName = df->name();
            stringstream line(dflowName);
            std::getline(line, full_path, ':');
            stringstream line2(full_path);
            std::getline(line2, filename, '/');
            dset = full_path.substr(full_path.find("/") + 1);
            dset = "/group1/" + dset; //TODO: discuss handling groups, specify also in the yaml file?

            // set ownership of dataset (default is user (shallow copy), lowfive means deep copy)
            // filename and full path to dataset can contain '*' and '?' wild cards (ie, globs, not regexes)
            if (df->ownership())
                vol_plugin.data_ownership(filename, dset, l5::Dataset::Ownership::lowfive);

         }
    }

    //I'm a consumer
    if (!node_in_dataflows.empty())
    {
        int index = 0;
        for (std::pair<Dataflow*, int> pair : node_in_dataflows)
        {

            dflowName = pair.first->name();
            stringstream line(dflowName);
            std::getline(line, full_path, ':');
            stringstream line2(full_path);
            std::getline(line2, filename, '/');
            dset = full_path.substr(full_path.find("/") + 1);
            dset = "/group1/" + dset; //TODO: discuss handling groups, specify also in the yaml file?

            // set intercomms of dataset
            // filename and full path to dataset can contain '*' and '?' wild cards (ie, globs, not regexes)
            //vol_plugin.data_intercomm(filename, dset, pair.second);
            vol_plugin.data_intercomm(filename, dset, index);
            index++;

            //orc@13-07: wait for data to be ready for the specific intercomm
            //orc@17-09: moving here since now we can have multiple intercomms for the same prod-con pair
	    //placing this after intercomm creation would result in a deadlock therefore
            if (passthru && !metadata)
                communicators[index-1].barrier();
        }

    }

    this->intercomms_ = communicators;
    this->out_intercomms_ = out_communicators;
    this->in_intercomms_ = in_communicators;

    return vol_plugin;
}

hid_t
wilkins::
Wilkins::plist()
{

return this->plist_;

}

//orc@14-07: signals that data is ready at lowfive prod
void
wilkins::
Wilkins::commit()
{

    int i = 0;
    for (Dataflow* df : out_dataflows)
    {
        if (df->out_passthru() && !df->out_metadata())
	    this->out_intercomms_[i].barrier();

        i++;
    }

}

int
wilkins::
Wilkins::prod_comm_size()
{
    if (!out_dataflows.empty())
        return out_dataflows[0]->sizes()->prod_size;
    else // The task is the only one in the graph
    {
        int size_comm;
        MPI_Comm_size(world_comm_, &size_comm);
        return size_comm;
    }
}

int
wilkins::
Wilkins::con_comm_size()
{
    if (!node_in_dataflows.empty())
        return node_in_dataflows[0].first->sizes()->con_size;
    else // The task is the only one in the graph
    {
        int size_comm;
        MPI_Comm_rank(world_comm_, &size_comm);
        return size_comm;
    }
}

int
wilkins::
Wilkins::local_comm_size()
{
    // We are the consumer in the inbound dataflow
    if (!node_in_dataflows.empty())
        return node_in_dataflows[0].first->sizes()->con_size;
    // We are the producer in the outbound dataflow
    else if(!out_dataflows.empty())
        return out_dataflows[0]->sizes()->prod_size;

    else
    {
        // We don't have nor inbound not outbound dataflows
        // The task is alone in the graph, returning world comm
        int comm_size;
        MPI_Comm_size(world_comm_, &comm_size);
        return comm_size;
    }


}

CommHandle
wilkins::
Wilkins::local_comm_handle()
{
    // We are the consumer in the inbound dataflow
    if (!node_in_dataflows.empty())
        return node_in_dataflows[0].first->con_comm_handle();
    // We are the producer in the outbound dataflow
    else if (!out_dataflows.empty())
        return out_dataflows[0]->prod_comm_handle();

    else
    {
        // We don't have nor inbound not outbound dataflows
        // The task is alone in the graph, returning world comm
        return world_comm_;
    }
}

int
wilkins::
Wilkins::local_comm_rank()
{
    int rank;
    MPI_Comm_rank(this->local_comm_handle(), &rank);
    return rank;
}

int
wilkins::
Wilkins::prod_comm_size(int i)
{
    if (node_in_dataflows.size() > i)
        return node_in_dataflows[i].first->sizes()->prod_size;

    return 0;
}

int
wilkins::
Wilkins::con_comm_size(int i)
{
    if (out_dataflows.size() > i)
        return out_dataflows[i]->sizes()->con_size;

    return 0;
}

int
wilkins::
Wilkins::workflow_comm_size()
{
   return workflow_size_;
}

int
wilkins::
Wilkins::workflow_comm_rank()
{
    return workflow_rank_;
}

