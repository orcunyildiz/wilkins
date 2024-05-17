#include <wilkins/workflow.hpp>
#include <string.h>
#include <numeric>

void
WorkflowNode::add_out_link(int link)
{
    out_links.push_back(link);
}

void
WorkflowNode::
add_in_link(int link)
{
    in_links.push_back(link);
}

bool
Workflow::my_node(int proc, int node)         // whether my process is part of this node
{
    return(proc >= nodes[node].start_proc &&
           proc <  nodes[node].start_proc + nodes[node].nprocs);
}

bool
Workflow::my_in_link(int proc, int link)      // whether my process gets input data from this link
{
    for (size_t i = 0; i< nodes.size(); i++)
    {
        if (proc >= nodes[i].start_proc && // proc is mine
                proc <  nodes[i].start_proc + nodes[i].nprocs)
        {
            for (size_t j = 0; j < nodes[i].in_links.size(); j++)
            {
                if (nodes[i].in_links[j] == link && links[link].name.find(nodes[i].func) != string::npos) //orc@21-10: rank is not enough to separate in TP mode, using also node func
                {
                    //fprintf(stderr, "my_in_link: node name is %s and link name is %s\n", nodes[i].func.c_str(), links[link].name.c_str());
                    return true;
                }
            }
        }
    }
    return false;
}

bool
Workflow::my_out_link(int proc, int link)      // whether my process puts output data to this link
{
    for (size_t i = 0; i< nodes.size(); i++)
    {
        if (proc >= nodes[i].start_proc && // proc is mine
                proc <  nodes[i].start_proc + nodes[i].nprocs)
        {
            for (size_t j = 0; j < nodes[i].out_links.size(); j++)
            {
                //orc@05-11: TODO: use fullName instead of name
                if (nodes[i].out_links[j] == link && links[link].name.find(nodes[i].func) == string::npos) //orc@21-10: rank is not enough to separate in TP mode, using also node func
                {
                    //fprintf(stderr, "my_OUT_link: node name is %s and link name is %s, and link src is %s\n", nodes[i].func.c_str(), links[link].name.c_str(), links[link].fullName.c_str());
                    return true;
                }
            }
        }
    }
    return false;
}

// ref: https://www.geeksforgeeks.org/wildcard-character-matching/
// checks if two given strings; the first string may contain wildcard characters
bool
match(const char *first, const char * second)
{
        if (*first == '\0' && *second == '\0')
            return true;

        if (*first == '*' && *(first+1) != '\0' && *second == '\0')
        {
            return false;
        }
        if (*first == '?' || *first == *second)
            return match(first+1, second+1);

        if (*first == '*')
            return match(first+1, second) || match(first, second+1);

        return false;
}
void
generateLinks (const vector<int> idx_task, const vector<int> idx_helper, int quot, Workflow& workflow, string k, int& l, int prod)
{
   int i = 0;
   for (const auto &idx : idx_task)
   {
        //for each filename+dset pair, we create a link
        WorkflowLink link;
        int p = i/quot;
        if (prod)
        {
            link.prod = -idx;
            link.con  = idx_helper[p];
        }
        else
        {
            link.prod = -1 * idx_helper[p];
            link.con = idx;
        }
        link.name = k + ":" + workflow.nodes.at(link.con).func;
        link.fullName = link.name + ":" + workflow.nodes.at(link.prod).func;
        link.execGroup = workflow.nodes.at(link.prod).func + ":" + workflow.nodes.at(link.con).func;
        //initializing in case user didn't specify
        link.out_passthru = 0;
        link.in_passthru  = 0;
        link.out_metadata = 1;
        link.in_metadata  = 1;

        for (LowFivePort outPort : workflow.nodes[link.prod].l5_outports) //orc@06-12: converting to l5_ports instead of ports.
        {
            string delim = ".";
            string postDlm_out = outPort.name.substr(outPort.name.find(delim), string::npos);

            string core_out;
            stringstream extendedOutPort(outPort.name);
            std::getline(extendedOutPort, core_out, '_');
            if (strcmp(outPort.name.c_str(),core_out.c_str()) != 0)
                core_out += postDlm_out; //orc: only extend this for fanin/fanout cases (once '_' found)

            if (match(k.c_str(),core_out.c_str()))
            {
                link.out_passthru = outPort.passthru;
                link.out_metadata = outPort.metadata;
                link.zerocopy     = outPort.zerocopy;
            }
        }

        for (LowFivePort inPort : workflow.nodes[link.con].l5_inports) //orc@06-12: converting to l5_ports instead of ports.
        {
            string delim = ".";
            string postDlm_in = inPort.name.substr(inPort.name.find(delim), string::npos);

            string idx = "_"; //TODO we can make this more wilkins specific even (e.g., "wilkins") since "_" could be used..
            string core_in;
            stringstream extendedInPort(inPort.name);
            std::getline(extendedInPort, core_in, '_');
            if (strcmp(inPort.name.c_str(),core_in.c_str()) != 0)
                core_in += postDlm_in; //orc: only extend this for fanin/fanout cases (once '_' found)

            if (match(k.c_str(),core_in.c_str()))
            {
                link.in_passthru = inPort.passthru;
                link.in_metadata = inPort.metadata;
                link.flow_policy = inPort.io_freq;
            }
        }

        //orc@08-12: handling conflicts btw prod/con pairs. Add more cases, if needed.
        if (link.in_passthru && !link.out_passthru)
        {
            fprintf(stderr, "Warning: Passthru is not enabled at the producer side, switching to metadata for %s.\n", k.c_str());
            link.in_passthru = 0;
            link.in_metadata = 1;
        }

        if (link.in_metadata && !link.out_metadata)
        {
            fprintf(stderr, "Warning: Metadata is not enabled at the producer side, switching to passthru for %s.\n", k.c_str());
            link.in_passthru = 1;
            link.in_metadata = 0;
        }

        //TODO think on how we handle the cycles
        link.tokens = 0;

        workflow.links.push_back( link );
        workflow.nodes.at( link.prod ).out_links.push_back(l);
        workflow.nodes.at( link.con ).in_links.push_back(l);
        l++;
        i++;

   }

}

void
Workflow::make_wflow_from_yaml( Workflow& workflow, const string& yaml_path )
{
    if (yaml_path.length() == 0)
    {
     	fprintf(stderr, "ERROR: No name filename provided for the YAML file. Unable to find the workflow graph definition.\n");
        exit(1);
    }

    try
    {
        YAML::Node root = YAML::LoadFile(yaml_path);
        const YAML::Node& nodes = root["tasks"];
        /*
        * iterate over the list of nodes, creating and populating WorkflowNodes as we go
        */

        int startProc = 0; //orc@10-03: eliminating start proc

        vector<int> file_range(100); //orc@15-04: used in subgraph API

        std::map<std::string, std::vector<int>> workflow_links;

        for (std::size_t i=0;i<nodes.size();i++)
        {
            //orc@08-03: adding the subgraph API
            int index = 0;

            int taskCount = 1;
            if(nodes[i]["taskCount"])
                taskCount = nodes[i]["taskCount"].as<int>();

            if (taskCount < 1)
            {
             	fprintf(stderr, "Error: task count cannot be smaller than 1\n");
                exit(1);
            }

            iota(file_range.begin(), file_range.end(), 1); //initializing in case user doesn't define in YAML

            for(std::size_t m=0; m<taskCount; m++)
            {
                WorkflowNode node;
                node.out_links.clear();
                node.in_links.clear();
                node.l5_inports.clear();
                node.l5_outports.clear();

                node.nprocs = nodes[i]["nprocs"].as<int>();
                node.nwriters = -1;
                if(nodes[i]["nwriters"])
                    node.nwriters = nodes[i]["nwriters"].as<int>();
                node.func =  nodes[i]["func"].as<std::string>();
                if(nodes[i]["args"])
                    node.args = nodes[i]["args"].as<std::vector<std::string>>();
                if(nodes[i]["actions"])
                    node.actions = nodes[i]["actions"].as<std::vector<std::string>>();

                node.taskCount = taskCount;
                if (taskCount > 1) node.func +=  "_" + to_string(index);
                //node.start_proc = nodes[i]["start_proc"].as<int>(); //orc@10-03: omitting start_proc, and calculating it ourselves instead
                node.start_proc = startProc;
                startProc += node.nprocs;

                if(nodes[i]["inports"])
                {
                    const YAML::Node& inports = nodes[i]["inports"];
                    for (std::size_t j=0;j<inports.size();j++)
                    {

                        string filename = inports[j]["filename"].as<std::string>();
                        string filename_orig = filename;

                        int io_freq = 1;
                        if(inports[j]["io_freq"])
                        {
                            try{
                                io_freq = inports[j]["io_freq"].as<int>();
                            }
                            catch (const std::exception& e) {
                                string flow_policy = inports[j]["io_freq"].as<std::string>();
                                if(flow_policy=="latest")
                                    io_freq = -1;
                                else{
                                    fprintf(stderr, "ERROR: %s -- Not supported flow control policy\n", flow_policy.c_str());
                                    exit(1);
                                }
                            }
                        }

                     	if(inports[j]["range"])
                            file_range = inports[j]["range"].as<std::vector<int>>();
                        //orc@24-03: this is to generate matching data reqs (dataflows) w subgraph API
                        //also using same filename convention on the user L5 code (since wilkins::init fetches the filename/dset from Workflow)
                        //alternatives: i) wilkins provides filenames to user for its H5 funcs ii) wilkins traps H5 calls to modify filename corresponding to these filenames w subgraph
                        //as of @18-04: we are obtaining this filename range via YAML, and providing to the user ourselves.
                        if (taskCount > 1)
                        {
                            string delim = ".";
                            string preDlm = filename.substr(0, filename.find(delim));
                            string postDlm = filename.substr(filename.find(delim), string::npos);
                            //filename = preDlm + "_" + to_string(index) + postDlm;
                            filename = preDlm + "_" + to_string(file_range[index]) + postDlm;
                        }
                        //orc@09-01: not sure whether dsets is optional or not
                        const YAML::Node& dsets = inports[j]["dsets"];
                        for (std::size_t k=0;k<dsets.size();k++)
                        {
                            string dset = dsets[k]["name"].as<std::string>();
                            string full_path = filename + "/" + dset; //dsets[k]["name"].as<std::string>();
                            string full_path_orig = filename_orig + "/" + dset; //dsets[k]["name"].as<std::string>();

                            //NB: first check whether there is a match before inserting with a different key (e.g., regex scenarios, out*.h5, outfile.h5)
                            int found = 0;
                            for (const auto &[k, v] : workflow_links)
                            {
                                if ( match(k.c_str(), full_path_orig.c_str()) && !found)
                                {
                                    if (workflow.nodes.size()==0)
                                        workflow_links[k].push_back(999);
                                    else
                                        workflow_links[k].push_back(workflow.nodes.size());
                                    found = 1;
                                }
                            }
                            if (!found)
                            {   //consumers have positive, producers have negative values
                                if (workflow.nodes.size()==0) // if node0 is a consumer, need to convert it to temporary positive value
                                    workflow_links[full_path_orig].push_back(999);
                                else
                                    workflow_links[full_path_orig].push_back(workflow.nodes.size());
                            }
                            //default values: p->0, m->1
                            int passthru  = 0;
                            int metadata  = 1;

                            if(dsets[k]["passthru"])
                                passthru = dsets[k]["passthru"].as<int>();
                            if(dsets[k]["metadata"])
                                metadata = dsets[k]["metadata"].as<int>();

                            if (!(metadata + passthru))
                            {
             	                fprintf(stderr, "Error: Either metadata or passthru must be enabled. Both cannot be disabled.\n");
                                exit(1);
                            }

                            LowFivePort l5_port;
                            l5_port.name      = full_path;
                            l5_port.filename  = filename;
                            l5_port.dset      = dset;
                            l5_port.passthru  = passthru;
                            l5_port.metadata  = metadata;
                            l5_port.io_freq   = io_freq;
                            node.l5_inports.push_back(l5_port);

                        }
                    }
                }


                if(nodes[i]["outports"])
                {
                    const YAML::Node& outports = nodes[i]["outports"];
                    for (std::size_t j=0;j<outports.size();j++)
                    {
                        string filename = outports[j]["filename"].as<std::string>();
                        string filename_orig = filename;

                        if(outports[j]["range"])
                            file_range = outports[j]["range"].as<std::vector<int>>();

                        if (taskCount > 1)
                        {
                            string delim = ".";
                            string preDlm = filename.substr(0, filename.find(delim));
                            string postDlm = filename.substr(filename.find(delim), string::npos);
                            //filename = preDlm + "_" + to_string(index) + postDlm;
                            filename = preDlm + "_" + to_string(file_range[index]) + postDlm;
                        }

                        const YAML::Node& dsets = outports[j]["dsets"];

                        for (std::size_t k=0;k<dsets.size();k++)
                        {
                            LowFivePort l5_port;

                            //default values: o->0, p->0, m->1
                            int zerocopy = 0;
                            int passthru  = 0;
                            int metadata  = 1;

                            string dset = dsets[k]["name"].as<std::string>();
                            string full_path = filename + "/" + dset; //dsets[k]["name"].as<std::string>();
                            string full_path_orig = filename_orig + "/" + dset; //dsets[k]["name"].as<std::string>();

                            //NB: first check whether there is a match before inserting with a different key (e.g., regex scenarios, out*.h5, outfile.h5)
                            int found = 0;
                            for (const auto &[k, v] : workflow_links)
                            {
                                if ( match(k.c_str(), full_path_orig.c_str()) && !found)
                                {
                                    workflow_links[k].push_back(-workflow.nodes.size());
                                    found = 1;
                                }
                            }
                            if (!found)
                                workflow_links[full_path_orig].push_back(-workflow.nodes.size()); //consumers have positive, producers have negative values

                            if(dsets[k]["zerocopy"])
                                zerocopy = dsets[k]["zerocopy"].as<int>();
                            if(dsets[k]["passthru"])
                                passthru = dsets[k]["passthru"].as<int>();
                            if(dsets[k]["metadata"])
                                metadata = dsets[k]["metadata"].as<int>();

                            if (!(metadata + passthru))
                            {
             	                fprintf(stderr, "Error: Either metadata or passthru must be enabled. Both cannot be disabled.\n");
                                exit(1);
                            }

			    l5_port.name      = full_path;
                            l5_port.filename  = filename;
                            l5_port.dset      = dset;
                            l5_port.zerocopy  = zerocopy;
                            l5_port.passthru  = passthru;
                            l5_port.metadata  = metadata;
                            node.l5_outports.push_back(l5_port);

                        }
                    }
                }

                workflow.nodes.push_back( node );
                index++;
             }// End for taskCount
         }// End for workflow.nodes

        //for each element create a link, in round-robin fashion
        int l = 0;
        for (const auto &[k, v] : workflow_links)
        {
            vector<int> idx_prod, idx_con;
            for (const auto &val : v)
            {
                if (val >0)
                {
                    if (val==999)
                        idx_con.push_back(0);
                    else
                        idx_con.push_back(val);
                }
                else
                {
                    idx_prod.push_back(val);
                }
            }

            //skip generating links (e.g., producer reading input file from disk, consumer writing output to disk--no coupling required)
            if (idx_prod.size()==0 || idx_con.size()==0)
            {
                if ( idx_prod.size()==0 ) // producer reading input file from disk
                {

                    for (LowFivePort inPort : workflow.nodes[idx_con[0]].l5_inports)
                    {
                        if ( match(k.c_str(), inPort.name.c_str()))
                        {
                            string fname = inPort.filename;
                            string dset = inPort.dset;
                            if (inPort.metadata)
                            {
                                fprintf(stderr, "ERROR: No matching link found for the inport %s%s requesting memory mode. Please use the file mode or specify a matching outport.\n",fname.c_str(), dset.c_str());
                                exit(1);
                            }
                            workflow.nodes[idx_con[0]].passthru_files.push_back(make_pair(fname, dset));
                        }
                    }

                }
                else // consumer writing output file to disk
                {

                    for (LowFivePort outPort : workflow.nodes[-idx_prod[0]].l5_outports)
                    {
                     	if ( match(k.c_str(), outPort.name.c_str()))
                        {
                            string fname = outPort.filename;
                            string dset = outPort.dset;
                            if (outPort.metadata)
                            {
                                fprintf(stderr, "ERROR: No matching link found for the outport %s%s requesting memory mode. Please use the file mode or specify a matching inport.\n", fname.c_str(), dset.c_str());
                                exit(1);
                            }
                            workflow.nodes[-idx_prod[0]].passthru_files.push_back(make_pair(fname, dset));
                        }
                    }


                }
            }
            else
            {
                int quot = (idx_prod.size() < idx_con.size()) ? idx_con.size()/idx_prod.size() : idx_prod.size() /idx_con.size();

                //iterating through the larger one
                if (idx_prod.size() <= idx_con.size())
                {
                    generateLinks(idx_con, idx_prod, quot, workflow, k, l, 0);
                }
                else
                {
                    generateLinks(idx_prod, idx_con, quot, workflow, k, l, 1);
                }
            }
        }

    }
    catch(YAML::Exception& e)
    {
        cerr << "YAML parser exception: " << e.what() << endl;
        exit(1);
    }

}

