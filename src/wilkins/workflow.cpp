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

                     	if(inports[j]["range"])
                            file_range = inports[j]["range"].as<std::vector<int>>();
                        //orc@24-03: this is to generate matching data reqs (dataflows) w subgraph API
                        //also using same filename convention on the user L5 code (since wilkins::init fetches the filename/dset from Workflow)
                        //alternatives: i) wilkins provides filenames to user for its H5 funcs ii) wilkins traps H5 calls to modify filename corresponding to these filenames w subgraph
                        //as of @18-04: wee are obtaining this filename range via YAML, and providing to the user ourselves.
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
                            string full_path = filename + "/" + dsets[k]["name"].as<std::string>();

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
                            l5_port.passthru  = passthru;
                            l5_port.metadata  = metadata;
                            node.l5_inports.push_back(l5_port);

                            //for each filename+dset pair, we create a link
                            WorkflowLink link;
                            link.prod = -1; //-1 indicates prod is undefined during the creation
                            link.con = workflow.nodes.size();
                            link.name = full_path + ":" + node.func;
                            link.in_passthru = passthru;
                            link.in_metadata = metadata;

                            link.zerocopy = 0;
                            //TODO think on how we handle the cycles
                            link.tokens = 0;

                            workflow.links.push_back( link );

                        }
                    }
                }


                if(nodes[i]["outports"])
                {
                    const YAML::Node& outports = nodes[i]["outports"];
                    for (std::size_t j=0;j<outports.size();j++)
                    {
                        string filename = outports[j]["filename"].as<std::string>();

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

                            string full_path = filename + "/" + dsets[k]["name"].as<std::string>(); //TODO: "/" could be redundant since this is specified in dset name already

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

        // linking the nodes //orc@21-03: w subgraph API, adding more linking options for fan-in and fan-out
        int size_links = workflow.links.size();
        int idx_links = workflow.links.size();
        //vector<string> vec_ic;
        for ( size_t i = 0; i< size_links; i++)
        {
            string inPort;
            stringstream line(workflow.links[i].name);
            std::getline(line, inPort, ':');
            int fanin_cnt = 0; //orc@21-03: used in fan-in for populating links (count per link)

            for (size_t j = 0; j < workflow.nodes.size(); j++)
            {
                //simply find the prod then connect it
                for (LowFivePort outPort : workflow.nodes[j].l5_outports) //orc@06-12: converting to l5_ports instead of ports.
                {

                    string delim = ".";
                    string postDlm_in = inPort.substr(inPort.find(delim), string::npos);
                    string postDlm_out = outPort.name.substr(outPort.name.find(delim), string::npos);

                    string idx = "_"; //TODO we can make this more wilkins specific even (e.g., "wilkins") since "_" could be used..
                    string core_in;
                    stringstream extendedInPort(inPort);
                    std::getline(extendedInPort, core_in, '_');
                    if (strcmp(inPort.c_str(),core_in.c_str()) != 0)
                        core_in += postDlm_in; //orc: only extend this for fanin/fanout cases (once '_' found)

                    string core_out;
                    stringstream extendedOutPort(outPort.name);
                    std::getline(extendedOutPort, core_out, '_');
                    if (strcmp(outPort.name.c_str(),core_out.c_str()) != 0)
                        core_out += postDlm_out; //orc: only extend this for fanin/fanout cases (once '_' found)

                    //orc@21-03: for fan-out, we are also checking whether the core outPort name (unmodified one w/o the index) still matching to the prod fullpath
                    //also making sure that we only do this for fan-out, and don't do for other cases like N-N (i.e., inPort.find(idx) != string::npos)
                    if(match(inPort.c_str(),outPort.name.c_str()) || ( match(core_in.c_str(),outPort.name.c_str()) && outPort.name.find(idx) == string::npos) )
                    {
                        workflow.links[i].prod = j;
                        workflow.links[i].out_passthru = outPort.passthru;
                        workflow.links[i].out_metadata = outPort.metadata;

			//orc@08-12: handling conflicts btw prod/con pairs. Add more cases, if needed.
                        if (workflow.links[i].in_passthru && !workflow.links[i].out_passthru)
                        {
                            fprintf(stderr, "Warning: Passthru is not enabled at the producer side, switching to metadata for %s.\n", outPort.name.c_str());
                            workflow.links[i].in_passthru = 0;
                            workflow.links[i].in_metadata = 1;
                        }

                        if (workflow.links[i].in_metadata && !workflow.links[i].out_metadata)
                        {
                            fprintf(stderr, "Warning: Metadata is not enabled at the producer side, switching to passthru for %s.\n", outPort.name.c_str());
                            workflow.links[i].in_passthru = 1;
                            workflow.links[i].in_metadata = 0;
                        }

                        workflow.links[i].zerocopy = outPort.zerocopy;
                        workflow.links[i].fullName = workflow.links[i].name + ":" + workflow.nodes[j].func; //orc@17-09: obtaining also prod name for the shared mode
                        //orc@05-12-22: Using execGroup to disable multiple intercomms per same prod-con pair.TODO: Need to check later how this plays out with the subgraph API.
                        string delim = ":";
                        string con = workflow.links[i].name.substr(workflow.links[i].name.find(delim), string::npos);
                        workflow.links[i].execGroup =  workflow.nodes[j].func + con; //TODO: We can include filename if we need: precede w outPort.filename + ":"
                        //fprintf(stderr, "this is execGroup name %s\n", workflow.links[i].execGroup.c_str());
                        //NB: We disable multiple intercomm generation at wilkins source, not at the workflow layer.
                        //if(find(vec_ic.begin(), vec_ic.end(), ic_name) ==  vec_ic.end())
                        //{
                            //vec_ic.push_back(ic_name);
                            workflow.nodes.at( j ).out_links.push_back(i);
                            workflow.nodes.at( workflow.links[i].con ).in_links.push_back(i);
                            //fprintf(stderr, "Match for a link %s with con %d and prod %d for link %d\n", workflow.links[i].fullName.c_str(), workflow.links[i].con, j, i);
                        //}
                    }
                    else if( match(core_out.c_str(), inPort.c_str()) && inPort.find(idx) == string::npos ) //orc@21-03: fan-in scenario
                    {
                     	fanin_cnt++;

                        if (fanin_cnt == 1)
                        {
                            //orc: using the existing link for fan-in (we created a single link so far)
                     	    string preDlm_link = workflow.links[i].name.substr(0, workflow.links[i].name.find(delim));
                            string postDlm_link = workflow.links[i].name.substr(workflow.links[i].name.find(delim), string::npos);
                            //workflow.links[i].name = preDlm_link + "_" + to_string(fanin_cnt-1) + postDlm_link;
                            workflow.links[i].name = preDlm_link + "_" + to_string(file_range[fanin_cnt-1]) + postDlm_link;

                     	    workflow.links[i].prod = j;
                            workflow.links[i].out_passthru = outPort.passthru;
                            workflow.links[i].out_metadata = outPort.metadata;

                            //orc@08-12: handling conflicts btw prod/con pairs. Add more cases, if needed.
                            if (workflow.links[i].in_passthru && !workflow.links[i].out_passthru)
                            {
                                fprintf(stderr, "Warning: Passthru is not enabled at the producer side, switching to metadata for %s.\n", outPort.name.c_str());
                                workflow.links[i].in_passthru = 0;
                                workflow.links[i].in_metadata = 1;
                            }

                            if (workflow.links[i].in_metadata && !workflow.links[i].out_metadata)
                            {
                                fprintf(stderr, "Warning: Metadata is not enabled at the producer side, switching to passthru for %s.\n", outPort.name.c_str());
                                workflow.links[i].in_passthru = 1;
                                workflow.links[i].in_metadata = 0;
                            }

                            workflow.links[i].zerocopy = outPort.zerocopy;
                            workflow.links[i].fullName = workflow.links[i].name + ":" + workflow.nodes[j].func;
                            workflow.nodes.at( j ).out_links.push_back(i);
                            workflow.nodes.at( workflow.links[i].con ).in_links.push_back(i);
                            //fprintf(stderr, "Match for a link %s with con %d and prod %d for link %d\n", workflow.links[i].fullName.c_str(), workflow.links[i].con, j, i);
                        }
                        else
                        {
                            //adding new links for fan-in:
                            WorkflowLink link;
                            link.prod = j;
                            link.out_passthru = outPort.passthru;
                            link.out_metadata = outPort.metadata;
                            link.zerocopy = outPort.zerocopy;
                            link.tokens = 0;

                            link.con = workflow.links[i].con;
                            link.name = outPort.name + ":" + workflow.nodes.at( workflow.links[i].con ).func;
                            link.fullName = link.name + ":" + workflow.nodes[j].func;
                            link.in_passthru = workflow.links[i].in_passthru;
                            link.in_metadata = workflow.links[i].in_metadata;

                            //orc@08-12: handling conflicts btw prod/con pairs. Add more cases, if needed.
                            if (link.in_passthru && !link.out_passthru)
                            {
                                fprintf(stderr, "Warning: Passthru is not enabled at the producer side, switching to metadata for %s.\n", outPort.name.c_str());
                                link.in_passthru = 0;
                                link.in_metadata = 1;
                            }

                            if (link.in_metadata && !link.out_metadata)
                            {
                                fprintf(stderr, "Warning: Metadata is not enabled at the producer side, switching to passthru for %s.\n", outPort.name.c_str());
                                link.in_passthru = 1;
                                link.in_metadata = 0;
                            }

                            workflow.nodes.at( j ).out_links.push_back(idx_links);
                            workflow.nodes.at( workflow.links[i].con ).in_links.push_back(idx_links);

                            workflow.links.push_back( link );
                            //fprintf(stderr, "Match for a link %s (fullname %s) with con %d and prod %d for link %d\n", link.name.c_str() ,link.fullName.c_str(), link.con, j, idx_links);
                            idx_links++;
                        }

                    }
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

