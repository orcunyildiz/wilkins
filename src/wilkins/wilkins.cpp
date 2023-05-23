#include <wilkins/wilkins.hpp>

#include <diy/mpi/communicator.hpp>
using diy_comm = diy::mpi::communicator;

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
    io_proc_ =  1;
    for (size_t i = 0; i < workflow_.links.size(); i++)
    {
        int prod  = workflow_.links[i].prod;    // index into workflow nodes
        int dflow = i;                          // index into workflow links
        int con   = workflow_.links[i].con;     // index into workflow nodes
        wilkins_sizes.prod_size           = workflow_.nodes[prod].nprocs;
        wilkins_sizes.prod_writers        = workflow_.nodes[prod].nwriters;
        wilkins_sizes.con_size            = workflow_.nodes[con].nprocs;
        wilkins_sizes.prod_start          = workflow_.nodes[prod].start_proc;
        wilkins_sizes.con_start           = workflow_.nodes[con].start_proc;

        dataflows.push_back(new Dataflow(world_comm_,
                                         workflow_size_,
                                         workflow_rank_,
                                         io_proc_,
                                         wilkins_sizes,
                                         prod,
                                         dflow,
                                         con,
                                         workflow_.links[i]));


    }
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


vector<LowFiveProperty>
wilkins::
Wilkins::set_lowfive()
{
    return this->vec_l5_;
}


void
wilkins::
Wilkins::wait()
{

    //I'm a consumer
    //orc@21-02: this is required for handshake assuming producer would finish with commit().
    if (!node_in_dataflows.empty())
    {
        int index = 0;
        std::vector<string> execGroup_dataflows;
        for (std::pair<Dataflow*, int> pair : node_in_dataflows)
        {
            //orc@05-12: adding this check after disabling multiple intercomms per execGroup
            if(std::find(execGroup_dataflows.begin(), execGroup_dataflows.end(), pair.first->execGroup()) == execGroup_dataflows.end())
            {
                execGroup_dataflows.push_back(pair.first->execGroup());
                index++; //orc@05-12: don't increment if you are in the same execGroup

                //orc@13-07: wait for data to be ready for the specific intercomm
                if (pair.first->in_passthru() && !pair.first->in_metadata())
                    diy_comm(this->in_intercomms_[index-1]).barrier();
            }

        }

    }

}


std::vector<int>
wilkins::
Wilkins::build_intercomms(std::string task_name)
{

    std::vector<int> shared_communicators;

    std::vector<std::string> shared_dataflows;
    //I'm a producer
    if (!out_dataflows.empty())
    {

     	for (Dataflow* df : out_dataflows)
        {
            if (df->sizes()->con_start == df->sizes()->prod_start)
            {
             	if(std::find(shared_dataflows.begin(), shared_dataflows.end(), df->name()) == shared_dataflows.end())
                {
                    shared_dataflows.push_back(df->name());
                    if(df->fullName().find(task_name) != std::string::npos)
                        shared_communicators.push_back(1);
                    else
                        shared_communicators.push_back(0);
                }
            }

	}

    }


    //I'm a consumer
    if (!node_in_dataflows.empty())
    {

        for (std::pair<Dataflow*, int> pair : node_in_dataflows)
        {

            if (pair.first->sizes()->prod_start == pair.first->sizes()->con_start)
            {
             	if(std::find(shared_dataflows.begin(), shared_dataflows.end(), pair.first->name()) == shared_dataflows.end()) //  && pair.first->fullName().find(task_name) != std::string::npos)
                {
                    shared_dataflows.push_back(pair.first->name());
                    if(pair.first->fullName().find(task_name) != std::string::npos)
                        shared_communicators.push_back(1);
                    else
                        shared_communicators.push_back(0);
                }
            }

        }

    }

    return shared_communicators;
}

std::vector<MPI_Comm>
wilkins::
Wilkins::build_intercomms()
{

    std::vector<MPI_Comm> communicators, out_communicators, in_communicators;
    MPI_Comm local_orig;
    local_orig = this->local_comm_handle();

    MPI_Comm local;
    MPI_Comm_dup(local_orig, &local);

    std::vector<std::string> execGroup_dataflows;

    int index = 0; //used at L5 set_intercomm for con and flowPolicy for prod
    std::vector<LowFiveProperty> vec_l5;
    std::string dflowName, full_path, filename, dset;
    //std::set<std::string> filenames; //orc: we can have multiple intercoms per filename. TODO: Will see about subgraph API later w wilkins.py

    int j=0;
    int k = 0;
    for (size_t i = 0; i < workflow_.links.size(); i++)
    {

        if (workflow_.my_in_link(workflow_rank_, i) || workflow_.my_out_link(workflow_rank_, i))
        {
            //orc@18-05: here we do in the order of links, rather than each task going thru their order
            if (workflow_.my_out_link(workflow_rank_, i))
            {
                dflowName = out_dataflows[j]->name();
                stringstream line(dflowName);
                std::getline(line, full_path, ':');
                stringstream line2(full_path);
                std::getline(line2, filename, '/');
                dset = full_path.substr(full_path.find("/") + 1);
                LowFiveProperty l5_prop;
                l5_prop.filename = filename;
                l5_prop.dset = dset;
                l5_prop.execGroup = out_dataflows[j]->execGroup();
                l5_prop.memory = 1;
                l5_prop.producer = 1;

                l5_prop.flowPolicy = out_dataflows[j]->flowPolicy();

                // set zerocopy of dataset (default is lowfive (deep copy), zerocopy means shallow copy)
                // filename and full path to dataset can contain '*' and '?' wild cards (ie, globs, not regexes)
                if (out_dataflows[j]->zerocopy())
                    l5_prop.zerocopy = 1; //orc: set_zerocopy is done at wilkins.py.

                if (out_dataflows[j]->out_passthru())
                    l5_prop.memory = 0;

                //filenames.insert(filename);

                if(std::find(execGroup_dataflows.begin(), execGroup_dataflows.end(), out_dataflows[j]->execGroup()) == execGroup_dataflows.end())
                {
                    execGroup_dataflows.push_back(out_dataflows[j]->execGroup());
                    MPI_Comm intercomm_;
                    if (out_dataflows[j]->sizes()->con_start == out_dataflows[j]->sizes()->prod_start) //TP
                    {
                        MPI_Comm_dup(local_orig, &intercomm_);
                    }
                    else //SP
                    {
                        int remote_leader = out_dataflows[j]->sizes()->con_start;
                        MPI_Intercomm_create(local, 0, world_comm_, remote_leader,  0, &intercomm_);
                    }

                    communicators.push_back(intercomm_);
                    out_communicators.push_back(intercomm_);
                    l5_prop.prodIndex = index;
                    index++;
                }
                else
                {
                    l5_prop.prodIndex = index-1;
                }

                j++; //NB: we still create dataflows, but not intercomm
                vec_l5.emplace_back(l5_prop);

            }
            else //incoming link
            {

                dflowName = node_in_dataflows[k].first->name();
                stringstream line(dflowName);
                std::getline(line, full_path, ':');
                stringstream line2(full_path);
                std::getline(line2, filename, '/');
                dset = full_path.substr(full_path.find("/") + 1);
                LowFiveProperty l5_prop;
                l5_prop.filename = filename;
                l5_prop.dset = dset;
                l5_prop.execGroup = node_in_dataflows[k].first->execGroup();
                l5_prop.memory = 1;
                l5_prop.consumer = 1;
                l5_prop.flowPolicy = node_in_dataflows[k].first->flowPolicy();

                // set passthru/memory at dataset level
                if (node_in_dataflows[k].first->in_passthru())
                    l5_prop.memory = 0;

                if(std::find(execGroup_dataflows.begin(), execGroup_dataflows.end(), node_in_dataflows[k].first->execGroup()) == execGroup_dataflows.end())
                {
                    execGroup_dataflows.push_back(node_in_dataflows[k].first->execGroup());
                    MPI_Comm intercomm_;

                    if (node_in_dataflows[k].first->sizes()->prod_start == node_in_dataflows[k].first->sizes()->con_start) //TP
                    {
                        MPI_Comm_dup(local_orig, &intercomm_); //orc@05-11: need duplicate intercomms for the shared mode
                    }
                    else //SP
                    {
                        int remote_leader = node_in_dataflows[k].first->sizes()->prod_start;
                        MPI_Intercomm_create(local, 0, world_comm_, remote_leader,  0, &intercomm_);
                    }

                    communicators.push_back(intercomm_);
                    in_communicators.push_back(intercomm_);
                    l5_prop.conIndex = index;
                    index++;
                }
                else //it's a link, but no intercomm created as they are within the same execGroup
                {
                    l5_prop.conIndex = index-1;
                }

                k++;
                vec_l5.emplace_back(l5_prop);

            }
        }
    }

    this->vec_l5_ = vec_l5;

    //orc@21-02: required for the commit function
    this->intercomms_ = communicators;
    this->out_intercomms_ = out_communicators;
    this->in_intercomms_ = in_communicators;

    return communicators;

}

vector<std::string>
wilkins::
Wilkins::filenames()
{

return this->filenames_;

}

//orc@14-07: signals that data is ready at lowfive prod
void
wilkins::
Wilkins::commit()
{

    std::vector<MPI_Comm> intercomms;

    std::vector<string> execGroup_dataflows;

    if (!wilkins_master())
        intercomms = this->out_intercomms_;
    else
        intercomms = wilkins_get_intercomms(); //TODO: for wilkins_master, use out_intercomms_ as well.

    int i = 0;
    for (Dataflow* df : out_dataflows)
    {

            //orc@05-12: adding this check after disabling multiple intercomms per execGroup.
            if(std::find(execGroup_dataflows.begin(), execGroup_dataflows.end(), df->execGroup()) == execGroup_dataflows.end())
            {
                execGroup_dataflows.push_back(df->execGroup());

                if (df->out_passthru() && !df->out_metadata())
                    diy_comm(intercomms[i]).barrier();

                i++;
            }
    }

}

int
wilkins::
Wilkins::is_io_proc()
{
    return this->io_proc_;
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

    if (wilkins_master())
    {
        return wilkins_get_local_comm();
    }

    // We are the producer in the outbound dataflow
    if (!out_dataflows.empty())
        return out_dataflows[0]->prod_comm_handle();
    // We are the consumer in the inbound dataflow
    else if (!node_in_dataflows.empty())
        return node_in_dataflows[0].first->con_comm_handle();
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

