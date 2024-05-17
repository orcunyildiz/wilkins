﻿//---------------------------------------------------------------------------
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
struct LowFivePort
{
    string name;
    string filename;
    string dset;
    int zerocopy;
    int passthru;
    int metadata;
    int io_freq;
};

struct WorkflowNode                          /// a producer or consumer
{
    WorkflowNode()                                {}
    WorkflowNode(int start_proc_,
                 int nprocs_,
                 string func_) :
        start_proc(start_proc_),
        nprocs(nprocs_),
        func(func_),
        nwriters(-1){}
    vector<int> out_links;      ///< indices of outgoing links
    vector<int> in_links;       ///< indices of incoming links
    int start_proc;             ///< starting processor rank (root) in world communicator for this producer or consumer
    int nprocs;                 ///< number of processes for this node (producer or consumer)
    int nwriters;               ///< (optional) number of writer processes for producer
    int taskCount;              ///< (optional) number of instances in ensembles
    string func;                ///< name of node callback
    vector<string> args;        ///< (optional) task arguments
    vector<string> actions;     ///< (optional) task actions

    //vector<string> inports;     ///< input ports, if available //orc@08-12: deprecated, using l5_inports instead
    //vector<string> outports;    ///< output ports, if available //orc@08-12: deprecated, using l5_outports instead
    vector<LowFivePort> l5_inports;     ///< input ports, if available
    vector<LowFivePort> l5_outports;    ///< output ports, if available

    vector<pair<string, string>> passthru_files;             ///< all the files without links (e.g., producer reading input file from disk)

    //int passthru;                   ///< "lowfive: write file to disk" //orc@08-12: deprecated as LowFive properties are moved to dset level
    //int metadata;                   ///< "lowfive: build and use in-memory metadata" //orc@08-12: deprecated as LowFive properties are moved to dset level
    void add_out_link(int link);
    void add_in_link(int link);
};

struct WorkflowLink                          /// a dataflow
{
    WorkflowLink()                                {}
    int prod;                       // index in vector of all workflow nodes of producer
    int con;                        // index in vector of all workflow nodes of consumer
    string name;                    ///< name of the link. Should be unique in the workflow
    string fullName;                ///< name of the link, which also includes source/producer.
    string execGroup;               ///< name of the execution group used to prevent multiple intercomms within this execGroup. Should be unique in the workflow

    int flow_policy;                ///< (optional) policy (io freq) for flow control
    int tokens;                     ///< number of empty messages to receive on destPort before a real get (for supporting cycles)
    int in_passthru;		    ///< "lowfive-con: write file to disk"
    int in_metadata;                ///< "lowfive-con: build and use in-memory metadata"
    int out_passthru;               ///< "lowfive-prod: write file to disk"
    int out_metadata;               ///< "lowfive-prod: build and use in-memory metadata"
    int zerocopy;                  // lowfive: set zerocopy of dataset (default (0) is lowfive (deep copy), 1 means shallow copy)

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
