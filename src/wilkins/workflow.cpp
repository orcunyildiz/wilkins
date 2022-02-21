#include <wilkins/workflow.hpp>

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
        const YAML::Node& nodes = root["nodes"];
        /*
        * iterate over the list of nodes, creating and populating WorkflowNodes as we go
        */
        for (std::size_t i=0;i<nodes.size();i++)
        {

            WorkflowNode node;
            node.out_links.clear();
            node.in_links.clear();
            //node.inports.clear();
            //node.outports.clear();
            node.l5_inports.clear();
            node.l5_outports.clear();

     	    node.start_proc = nodes[i]["start_proc"].as<int>();
            node.nprocs = nodes[i]["nprocs"].as<int>();
            node.func =  nodes[i]["func"].as<std::string>();

            //orc@08-12: moving passthru/metadata flags back to edges (i.e., dataset level)
/*
            //lowfive related flags
            if(nodes[i]["passthru"])
                node.passthru = nodes[i]["passthru"].as<int>();
            else
                node.passthru = 0;

            if(nodes[i]["metadata"])
                node.metadata = nodes[i]["metadata"].as<int>();
            else
                node.metadata = 1;

            if (!(node.metadata + node.passthru))
            {
                fprintf(stderr, "Error: Either metadata or passthru must be enabled. Both cannot be disabled.\n");
                exit(1);
            }
*/
            if(nodes[i]["inports"])
            {
                const YAML::Node& inports = nodes[i]["inports"];
                for (std::size_t j=0;j<inports.size();j++)
                {

                    string filename = inports[j]["filename"].as<std::string>();
                    //orc@09-01: not sure whether dsets is optional or not
                    const YAML::Node& dsets = inports[j]["dsets"];
                    for (std::size_t k=0;k<dsets.size();k++)
                    {
                            string full_path = filename + "/" + dsets[k]["name"].as<std::string>();
                            //node.inports.push_back(full_path);

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

                            link.ownership = 0;
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

                    const YAML::Node& dsets = outports[j]["dsets"];

                    for (std::size_t k=0;k<dsets.size();k++)
                    {
                        LowFivePort l5_port;

                        //default values: o->0, p->0, m->1
                        int ownership = 0;
                        int passthru  = 0;
                        int metadata  = 1;

                        string full_path = filename + "/" + dsets[k]["name"].as<std::string>();
                        //node.outports.push_back(full_path); //deprecated, TODO: omit this once L5 design is finalized

                        if(dsets[k]["ownership"])
                            ownership = dsets[k]["ownership"].as<int>();
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
                        l5_port.ownership = ownership;
                        l5_port.passthru  = passthru;
                        l5_port.metadata  = metadata;
                        node.l5_outports.push_back(l5_port);

                    }
                }
            }

            workflow.nodes.push_back( node );

         }// End for workflow.nodes

       // linking the nodes
        for ( size_t i = 0; i< workflow.links.size(); i++)
        {
            string inPort;
            stringstream line(workflow.links[i].name);
            std::getline(line, inPort, ':');

            for (size_t j = 0; j < workflow.nodes.size(); j++)
            {
                //simply find the prod then connect it
                //for (string outPort : workflow.nodes[j].outports)
                for (LowFivePort outPort : workflow.nodes[j].l5_outports) //orc@06-12: converting to l5_ports instead of ports.
                {
                    if(match(inPort.c_str(),outPort.name.c_str()))
                    {
                        workflow.links[i].prod = j;
                        workflow.links[i].out_passthru = outPort.passthru;
                        workflow.links[i].out_metadata = outPort.metadata;
			//orc@08-12: handling conflicts btw prod/con pairs. Add more cases, if needed.
                        if (workflow.links[i].in_passthru && !workflow.links[i].out_passthru)
                        {
                            fprintf(stderr, "Error: Passthru is not enabled at the producer side, switching to metadata for %s.\n", outPort.name.c_str());
                            workflow.links[i].in_passthru = 0;
                            workflow.links[i].in_metadata = 1;
                        }

                        if (workflow.links[i].in_metadata && !workflow.links[i].out_metadata)
                        {
                            fprintf(stderr, "Error: Metadata is not enabled at the producer side, switching to passthru for %s.\n", outPort.name.c_str());
                            workflow.links[i].in_passthru = 1;
                            workflow.links[i].in_metadata = 0;
                        }

                        workflow.links[i].ownership = outPort.ownership;
                        workflow.links[i].fullName = workflow.links[i].name + ":" + workflow.nodes[j].func; //orc@17-09: obtaining also prod name for the shared mode
                        workflow.nodes.at( j ).out_links.push_back(i);
                        workflow.nodes.at( workflow.links[i].con ).in_links.push_back(i);
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

