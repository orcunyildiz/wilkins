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
            /* we defer actually linking nodes until we read the edge list */

            node.start_proc = v.second.get<int>("start_proc");
            node.nprocs = v.second.get<int>("nprocs");
            node.func = v.second.get<string>("func");

            // Retrieving the input ports, if present
            boost::optional<bpt::ptree&> pt_inputs = v.second.get_child_optional("inports");
            if (pt_inputs)
            {
                for (auto& item : pt_inputs->get_child(""))
                    node.inports.push_back(item.second.get_value<string>());
            }

            // Retrieving the output ports, if present
            boost::optional<bpt::ptree&> pt_outputs = v.second.get_child_optional("outports");
            if (pt_outputs)
            {
                for (auto& item : pt_outputs->get_child(""))
                    node.outports.push_back(item.second.get_value<string>());
            }

            workflow.nodes.push_back( node );
        } // End for workflow.nodes

        /*
        * similarly for the edges
        */
        for ( auto &&v : root.get_child( "workflow.edges" ) )
        {
            WorkflowLink link;

            /* link the nodes correctly(?) */
            link.prod = v.second.get<int>("source");
            link.con = v.second.get<int>("target");

            workflow.nodes.at( link.prod ).out_links.push_back( workflow.links.size() );
            workflow.nodes.at( link.con ).in_links.push_back( workflow.links.size() );

            link.name = v.second.get<string>("name");
            //orc@13-07: adding lowfive passthru&metadata&ownership flags
            boost::optional<int> opt_passthru = v.second.get_optional<int>("passthru");
            if(opt_passthru)
                link.passthru = opt_passthru.get();
            else
                link.passthru = 0;
            boost::optional<int> opt_metadata = v.second.get_optional<int>("metadata");
            if(opt_metadata)
                link.metadata = opt_metadata.get();
            else
                link.metadata = 1;
            boost::optional<int> opt_ownership = v.second.get_optional<int>("ownership");
            if(opt_ownership)
                link.ownership = opt_ownership.get();
            else
                link.ownership = 0;
            boost::optional<int> opt_tokens = v.second.get_optional<int>("tokens");
            if(opt_tokens)
                link.tokens = opt_tokens.get();
            else
                link.tokens = 0;

            workflow.links.push_back( link );
        } // End for workflow.links
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
