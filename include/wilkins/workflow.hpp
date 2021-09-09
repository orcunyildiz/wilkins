//---------------------------------------------------------------------------
//
// workflow definition
//
// Tom Peterka
// Argonne National Laboratory
// 9700 S. Cass Ave.
// Argonne, IL 60439
// tpeterka@mcs.anl.gov
//
//--------------------------------------------------------------------------

#ifndef WILKINS_WORKFLOW_HPP
#define WILKINS_WORKFLOW_HPP

#include <stdio.h>
#include <vector>
#include <queue>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

#include <yaml-cpp/yaml.h>

#include <string>

using namespace std;

struct WorkflowNode                          /// a producer or consumer
{
    WorkflowNode()                                {}
    WorkflowNode(int start_proc_,
                 int nprocs_,
                 string func_) :
        start_proc(start_proc_),
        nprocs(nprocs_),
        func(func_),
        args(NULL){}
    vector<int> out_links;      ///< indices of outgoing links
    vector<int> in_links;       ///< indices of incoming links
    int start_proc;             ///< starting processor rank (root) in world communicator for this producer or consumer
    int nprocs;                 ///< number of processes for this node (producer or consumer)
    string func;                ///< name of node callback
    void* args;                 ///< callback arguments
    vector<string> inports;     ///< input ports, if available
    vector<string> outports;    ///< output ports, if available
    int passthru;                   ///< "lowfive: write file to disk"
    int metadata;                   ///< "lowfive: build and use in-memory metadata"
    void add_out_link(int link);
    void add_in_link(int link);
};

//orc@17-08: TODO clean further at the wilkins.py as well
struct WorkflowLink                          /// a dataflow
{
    WorkflowLink()                                {}
    int prod;                       // index in vector of all workflow nodes of producer
    int con;                        // index in vector of all workflow nodes of consumer
    string name;                    ///< name of the link. Should be unique in the workflow

    int tokens;                     ///< number of empty messages to receive on destPort before a real get (for supporting cycles)
    int in_passthru;		    ///< "lowfive-con: write file to disk"
    int in_metadata;                ///< "lowfive-con: build and use in-memory metadata"
    int out_passthru;               ///< "lowfive-prod: write file to disk"
    int out_metadata;               ///< "lowfive-prod: build and use in-memory metadata"
    int ownership;                  // lowfive: set ownership of dataset (default (0) is user (shallow copy), 1 means deep copy)

};

struct Workflow                              /// an entire workflow
{
    Workflow()                                    {}
    Workflow(vector<WorkflowNode>& nodes_,
             vector<WorkflowLink>& links_) :
        nodes(nodes_),
        links(links_)                             {}
    vector<WorkflowNode> nodes;             ///< all the workflow nodes
    vector<WorkflowLink> links;             ///< all the workflow links
    bool my_node(int proc, int node);       ///< whether my process is part of this node

    bool my_in_link(int proc, int link);    ///< whether my process gets input data from this link

    bool my_out_link(int proc, int link);   ///< whether my process puts output data to this link

    static void
    make_wflow_from_yaml( Workflow& workflow, const string& yaml_path );
};

#endif
