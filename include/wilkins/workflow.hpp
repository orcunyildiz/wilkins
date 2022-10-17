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

//orc@04-01: borrowing serialization from DH for the workflow object
#include <wilkins/serialization.hpp>

using namespace std;
struct LowFivePort
{
    string name;
    int ownership;
    int passthru;
    int metadata;
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
        args(NULL){}
    vector<int> out_links;      ///< indices of outgoing links
    vector<int> in_links;       ///< indices of incoming links
    int start_proc;             ///< starting processor rank (root) in world communicator for this producer or consumer
    int nprocs;                 ///< number of processes for this node (producer or consumer)
    string func;                ///< name of node callback
    void* args;                 ///< callback arguments
    //vector<string> inports;     ///< input ports, if available //orc@08-12: deprecated, using l5_inports instead
    //vector<string> outports;    ///< output ports, if available //orc@08-12: deprecated, using l5_outports instead
    vector<LowFivePort> l5_inports;     ///< input ports, if available
    vector<LowFivePort> l5_outports;    ///< output ports, if available
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


namespace wilkins
{

template<>
struct Serialization<WorkflowNode>
{
    static void         save(BinaryBuffer& bb, const WorkflowNode& j)
    {
        wilkins::save(bb, j.start_proc);
        wilkins::save(bb, j.nprocs);
        wilkins::save(bb, j.func);
        //TODO: add LowFivePort as well for l5_inports and l5_outports?
    }

    static void         load(BinaryBuffer& bb, WorkflowNode& j)
    {
     	wilkins::load(bb, j.start_proc);
        wilkins::load(bb, j.nprocs);
        wilkins::load(bb, j.func);
    }
};


template<>
struct Serialization<WorkflowLink>
{
    static void         save(BinaryBuffer& bb, const WorkflowLink& j)
    {
        wilkins::save(bb, j.prod);
        wilkins::save(bb, j.con);
        wilkins::save(bb, j.name);
        wilkins::save(bb, j.fullName);
        wilkins::save(bb, j.tokens);
        wilkins::save(bb, j.in_passthru);
        wilkins::save(bb, j.in_metadata);
        wilkins::save(bb, j.out_passthru);
        wilkins::save(bb, j.out_metadata);
        wilkins::save(bb, j.ownership);
    }

    static void         load(BinaryBuffer& bb, WorkflowLink& j)
    {
     	wilkins::load(bb, j.prod);
        wilkins::load(bb, j.con);
        wilkins::load(bb, j.name);
        wilkins::load(bb, j.fullName);
        wilkins::load(bb, j.tokens);
        wilkins::load(bb, j.in_passthru);
        wilkins::load(bb, j.in_metadata);
        wilkins::load(bb, j.out_passthru);
        wilkins::load(bb, j.out_metadata);
        wilkins::load(bb, j.ownership);
    }
};

template<>
struct Serialization<Workflow>
{
    static void         save(BinaryBuffer& bb, const Workflow& j)
    {
     	wilkins::save(bb, j.nodes);
        wilkins::save(bb, j.links);

    }

    static void         load(BinaryBuffer& bb, Workflow& j)
    {
     	wilkins::load(bb, j.nodes);
        wilkins::load(bb, j.links);
    }
};

}

#endif
