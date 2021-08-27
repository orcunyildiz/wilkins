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
                if (nodes[i].in_links[j] == link)
                    return true;
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
                if (nodes[i].out_links[j] == link)
                    return true;
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
Workflow::make_wflow_from_json( Workflow& workflow, const string& json_path )
{
    string json_filename = json_path;
    if (json_filename.length() == 0)
    {
        fprintf(stderr, "ERROR: No name filename provided for the JSON file. Unable to find the workflow graph definition.\n");
        exit(1);
    }

    try {

        bpt::ptree root;

        /*
       * Use Boost::property_tree to read/parse the JSON file. This
       * creates a property_tree object which we'll then query to get
       * the information we want.
       *
       * N.B. Unlike what is provided by most JSON/XML parsers, the
       * property_tree is NOT a DOM tree, although processing it is somewhat
       * similar to what you'd do with a DOM tree. Refer to the Boost documentation
       * for more information.
       */

        bpt::read_json( json_filename, root );

        /*
        * iterate over the list of nodes, creating and populating WorkflowNodes as we go
        */
        for ( auto &&v : root.get_child( "workflow.nodes" ) )
        {
            WorkflowNode node;
            node.out_links.clear();
            node.in_links.clear();
            node.inports.clear();
            node.outports.clear();

            node.start_proc = v.second.get<int>("start_proc");
            node.nprocs = v.second.get<int>("nprocs");
            node.func = v.second.get<string>("func");

            //moving passthru&metadata flags to the nodes
            boost::optional<int> opt_passthru = v.second.get_optional<int>("passthru");
            if(opt_passthru)
                node.passthru = opt_passthru.get();
            else
                node.passthru = 0;
            boost::optional<int> opt_metadata = v.second.get_optional<int>("metadata");
            if(opt_metadata)
                node.metadata = opt_metadata.get();
            else
                node.metadata = 1;

            if (!(node.metadata + node.passthru))
            {
                fprintf(stderr, "Error: Either metadata or passthru must be enabled. Both cannot be disabled.\n");
                exit(1);
            }

            // Retrieving the input ports, if present
            boost::optional<bpt::ptree&> pt_inputs = v.second.get_child_optional("inports");
            if (pt_inputs)
            {
                for (auto& item : pt_inputs->get_child(""))
                {

                    string filename = item.second.get<string>("filename");
                    // Retrieving the input datasets, if present
                    boost::optional<bpt::ptree&> pt_dsets = item.second.get_child_optional("dsets");
                    if (pt_dsets)
                    {
                     	for (auto& item : pt_dsets->get_child("")) {
                            string full_path = filename + "/" + item.second.get_value<string>();
                            node.inports.push_back(full_path);
                            //for each filename+dset pair, we create a link
                            WorkflowLink link;
                            link.prod = -1; //-1 indicates prod is undefined during the creation
                    	    link.con = workflow.nodes.size();
                            link.name = full_path + ":" + node.func;
                            //TODO move these to nodes rather than edges:
                            link.in_passthru = node.passthru;
                            link.in_metadata = node.metadata;

                            link.ownership = 0;
                            //TODO think on how we handle the cycles
                            link.tokens = 0;

                            workflow.links.push_back( link );

                        }
                    }

                }
            }

            boost::optional<bpt::ptree&> pt_outports = v.second.get_child_optional("outports");
            if (pt_outports)
            {
             	for (auto& item : pt_outports->get_child(""))
                {
                    int ownership = 0;
                    boost::optional<int> opt_ownership = item.second.get_optional<int>("ownership");
                    if(opt_ownership)
                        ownership = opt_ownership.get();

                    string filename = item.second.get<string>("filename");
                    // Retrieving the output datasets, if present
                    boost::optional<bpt::ptree&> pt_dsets = item.second.get_child_optional("dsets");
                    if (pt_dsets)
                    {
             	        for (auto& item : pt_dsets->get_child("")) {
                            string full_path = filename + "/" + item.second.get_value<string>();
                            node.outports.push_back(full_path); //TODO: add the ownership info later
                        }
                    }

                }
            }

            workflow.nodes.push_back( node );
        } // End for workflow.nodes

        // linking the nodes
        for ( size_t i = 0; i< workflow.links.size(); i++)
        {
            string inPort;
            stringstream line(workflow.links[i].name);
            std::getline(line, inPort, ':');

            for (size_t j = 0; j < workflow.nodes.size(); j++)
            {
	        //simply find the prod then connect it
                for (string outPort : workflow.nodes[j].outports)
                {
                    if(match(inPort.c_str(),outPort.c_str()))
                    {
                        workflow.links[i].prod = j;
                     	workflow.links[i].out_passthru = workflow.nodes[j].passthru;
                        workflow.links[i].out_metadata = workflow.nodes[j].metadata;

                        workflow.nodes.at( j ).out_links.push_back(i);
                        workflow.nodes.at( workflow.links[i].con ).in_links.push_back(i);
                    }
                }
            }

        }

    }
    catch( const bpt::json_parser_error& jpe )
    {
        cerr << "JSON parser exception: " << jpe.what() << endl;
        exit(1);
    }
    catch ( const bpt::ptree_error& pte )
    {
        cerr << "property_tree exception: " << pte.what() << endl;
        exit(1);
    }

}
