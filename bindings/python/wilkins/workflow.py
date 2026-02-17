# ---------------------------------------------------------------------------
#
# workflow definition -- Python port of include/wilkins/workflow.hpp
#                        and src/wilkins/workflow.cpp
#
# --------------------------------------------------------------------------

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from fnmatch import fnmatch
from typing import List, Tuple

import yaml


# --------------------------------------------------------------------------
# Wildcard string matching (port of match() from workflow.cpp)
# Supports '*' and '?' characters.
# ref: https://www.geeksforgeeks.org/wildcard-character-matching/
# --------------------------------------------------------------------------

def _match(pattern: str, text: str) -> bool:
    """Check if *text* matches *pattern* which may contain '*' and '?'."""
    return fnmatch(text, pattern)


# --------------------------------------------------------------------------
# Data structures
# --------------------------------------------------------------------------

@dataclass
class LowFivePort:
    """A single input or output port with LowFive properties."""
    name: str = ""           # full path: filename/dataset
    filename: str = ""       # HDF5 filename
    dset: str = ""           # dataset path within HDF5 file
    zerocopy: int = 0        # whether to use zerocopy (shallow) transfer
    passthru: int = 0        # whether to write to disk (file mode)
    metadata: int = 1        # whether to use in-memory metadata mode
    io_freq: int = 1         # I/O frequency for flow control


@dataclass
class WorkflowNode:
    """A producer or consumer task in the workflow graph."""
    out_links: List[int] = field(default_factory=list)
    in_links: List[int] = field(default_factory=list)
    start_proc: int = 0          # starting processor rank (root) in world communicator
    nprocs: int = 0              # number of processes for this node
    nwriters: int = -1           # (optional) number of writer processes for producer
    taskCount: int = 1           # (optional) number of instances in ensembles
    func: str = ""               # name of node callback
    args: List[str] = field(default_factory=list)      # (optional) task arguments
    actions: List[str] = field(default_factory=list)    # (optional) task actions
    l5_inports: List[LowFivePort] = field(default_factory=list)
    l5_outports: List[LowFivePort] = field(default_factory=list)
    passthru_files: List[Tuple[str, str]] = field(default_factory=list)

    def add_out_link(self, link: int):
        self.out_links.append(link)

    def add_in_link(self, link: int):
        self.in_links.append(link)


@dataclass
class WorkflowLink:
    """A dataflow edge in the workflow graph."""
    prod: int = 0                # index in vector of all workflow nodes of producer
    con: int = 0                 # index in vector of all workflow nodes of consumer
    name: str = ""               # name of the link (should be unique)
    fullName: str = ""           # name of the link including source/producer
    execGroup: str = ""          # execution group name
    flow_policy: int = 1         # (optional) policy (io freq) for flow control
    tokens: int = 0              # number of empty messages for cycle support
    in_passthru: int = 0         # lowfive-con: write file to disk
    in_metadata: int = 1         # lowfive-con: build and use in-memory metadata
    out_passthru: int = 0        # lowfive-prod: write file to disk
    out_metadata: int = 1        # lowfive-prod: build and use in-memory metadata
    zerocopy: int = 0            # lowfive: zerocopy of dataset (0=deep, 1=shallow)


class Workflow:
    """An entire workflow graph."""

    def __init__(self, nodes=None, links=None):
        self.nodes: List[WorkflowNode] = nodes if nodes is not None else []
        self.links: List[WorkflowLink] = links if links is not None else []

    def my_node(self, proc: int, node: int) -> bool:
        """Whether the given process rank belongs to the given node."""
        n = self.nodes[node]
        return proc >= n.start_proc and proc < n.start_proc + n.nprocs

    def my_in_link(self, proc: int, link: int) -> bool:
        """Whether the given process gets input data from this link."""
        for i, n in enumerate(self.nodes):
            if proc >= n.start_proc and proc < n.start_proc + n.nprocs:
                for j in n.in_links:
                    # rank is not enough to separate in TP mode, using also node func
                    if j == link and n.func in self.links[link].name:
                        return True
        return False

    def my_out_link(self, proc: int, link: int) -> bool:
        """Whether the given process puts output data to this link."""
        for i, n in enumerate(self.nodes):
            if proc >= n.start_proc and proc < n.start_proc + n.nprocs:
                for j in n.out_links:
                    # rank is not enough to separate in TP mode, using also node func
                    if j == link and n.func not in self.links[link].name:
                        return True
        return False

    def make_wflow_from_yaml(self, yaml_path=None):
        """Parse YAML config file and populate the workflow graph.

        Supports both calling conventions:
          - ``Workflow.make_wflow_from_yaml(workflow, yaml_path)``  (unbound)
          - ``workflow.make_wflow_from_yaml(yaml_path)``            (bound)
        """
        if yaml_path is None:
            raise TypeError(
                "make_wflow_from_yaml() missing required argument: 'yaml_path'"
            )
        _parse_yaml(self, yaml_path)


# --------------------------------------------------------------------------
# Link generation (port of generateLinks() from workflow.cpp)
# --------------------------------------------------------------------------

def _generate_links(idx_task, idx_helper, quot, workflow, k, l, prod):
    """Generate WorkflowLink objects in round-robin fashion.

    Args:
        idx_task:   indices of the larger group (consumers or producers)
        idx_helper: indices of the smaller group
        quot:       ratio of larger/smaller group sizes
        workflow:   Workflow object to populate
        k:          filename+dset key
        l:          current link index (mutated via return)
        prod:       1 if idx_task contains producers, 0 if consumers

    Returns:
        Updated link index ``l``.
    """
    i = 0
    for idx in idx_task:
        link = WorkflowLink()
        p = i // quot

        if prod:
            link.prod = -idx
            link.con = idx_helper[p]
        else:
            link.prod = -1 * idx_helper[p]
            link.con = idx

        link.name = k + ":" + workflow.nodes[link.con].func
        link.fullName = link.name + ":" + workflow.nodes[link.prod].func
        link.execGroup = workflow.nodes[link.prod].func + ":" + workflow.nodes[link.con].func

        # initialize in case user didn't specify
        link.out_passthru = 0
        link.in_passthru = 0
        link.out_metadata = 1
        link.in_metadata = 1

        # Match outports
        for out_port in workflow.nodes[link.prod].l5_outports:
            delim = "."
            dot_pos = out_port.name.find(delim)
            post_dlm_out = out_port.name[dot_pos:] if dot_pos != -1 else ""

            delimiter = "-inst"
            full_name = out_port.name
            pos = full_name.find(delimiter)

            if pos != -1:
                core_out = full_name[:pos] + post_dlm_out
            else:
                core_out = full_name

            if _match(k, core_out):
                link.out_passthru = out_port.passthru
                link.out_metadata = out_port.metadata
                link.zerocopy = out_port.zerocopy

        # Match inports
        for in_port in workflow.nodes[link.con].l5_inports:
            delim = "."
            dot_pos = in_port.name.find(delim)
            post_dlm_in = in_port.name[dot_pos:] if dot_pos != -1 else ""

            delimiter = "-inst"
            full_name = in_port.name
            pos = full_name.find(delimiter)

            if pos != -1:
                core_in = full_name[:pos] + post_dlm_in
            else:
                core_in = full_name

            if _match(k, core_in):
                link.in_passthru = in_port.passthru
                link.in_metadata = in_port.metadata
                link.flow_policy = in_port.io_freq

        # Handle conflicts between prod/con pairs
        if link.in_passthru and not link.out_passthru:
            print(
                f"Warning: Passthru is not enabled at the producer side, "
                f"switching to metadata for {k}.",
                file=sys.stderr,
            )
            link.in_passthru = 0
            link.in_metadata = 1

        if link.in_metadata and not link.out_metadata:
            print(
                f"Warning: Metadata is not enabled at the producer side, "
                f"switching to passthru for {k}.",
                file=sys.stderr,
            )
            link.in_passthru = 1
            link.in_metadata = 0

        link.tokens = 0

        workflow.links.append(link)
        workflow.nodes[link.prod].out_links.append(l)
        workflow.nodes[link.con].in_links.append(l)
        l += 1
        i += 1

    return l


# --------------------------------------------------------------------------
# YAML parser (port of make_wflow_from_yaml() from workflow.cpp)
# --------------------------------------------------------------------------

def _parse_yaml(workflow: Workflow, yaml_path: str):
    """Parse a YAML workflow configuration file and populate the workflow."""
    if not yaml_path:
        print(
            "ERROR: No name filename provided for the YAML file. "
            "Unable to find the workflow graph definition.",
            file=sys.stderr,
        )
        sys.exit(1)

    try:
        with open(yaml_path, "r") as f:
            root = yaml.safe_load(f)
    except yaml.YAMLError as e:
        print(f"YAML parser exception: {e}", file=sys.stderr)
        sys.exit(1)

    nodes_yaml = root["tasks"]
    start_proc = 0
    file_range = list(range(1, 101))  # default: [1, 2, ..., 100]

    # Maps filename+dset to list of node indices.
    # Consumers have positive values, producers have negative values.
    workflow_links = {}  # ordered dict (Python 3.7+)

    for i, task_yaml in enumerate(nodes_yaml):
        task_count = task_yaml.get("taskCount", 1)
        if task_count < 1:
            print("Error: task count cannot be smaller than 1", file=sys.stderr)
            sys.exit(1)

        # Reset file_range for each task definition
        file_range = list(range(1, 101))

        for m in range(task_count):
            node = WorkflowNode()

            node.nprocs = task_yaml["nprocs"]
            node.nwriters = task_yaml.get("nwriters", -1)
            node.func = task_yaml["func"]
            node.args = task_yaml.get("args", [])
            node.actions = task_yaml.get("actions", [])
            node.taskCount = task_count

            index = m
            if task_count > 1:
                node.func += f"-inst{index}"

            node.start_proc = start_proc
            start_proc += node.nprocs

            # -- Parse inports --
            if "inports" in task_yaml:
                for inport_yaml in task_yaml["inports"]:
                    filename = inport_yaml["filename"]
                    filename_orig = filename

                    # Parse io_freq
                    io_freq = 1
                    if "io_freq" in inport_yaml:
                        raw_freq = inport_yaml["io_freq"]
                        if isinstance(raw_freq, int):
                            io_freq = raw_freq
                        elif isinstance(raw_freq, str):
                            if raw_freq == "latest":
                                io_freq = -1
                            else:
                                print(
                                    f"ERROR: {raw_freq} -- Not supported flow control policy",
                                    file=sys.stderr,
                                )
                                sys.exit(1)
                        else:
                            io_freq = int(raw_freq)

                    if "range" in inport_yaml:
                        file_range = inport_yaml["range"]

                    # Apply ensemble filename convention
                    if task_count > 1:
                        dot_pos = filename.find(".")
                        if dot_pos != -1:
                            pre_dlm = filename[:dot_pos]
                            post_dlm = filename[dot_pos:]
                        else:
                            pre_dlm = filename
                            post_dlm = ""
                        filename = f"{pre_dlm}-inst{file_range[index]}{post_dlm}"

                    # Parse datasets
                    for dset_yaml in inport_yaml["dsets"]:
                        dset = dset_yaml["name"]
                        full_path = f"{filename}/{dset}"
                        full_path_orig = f"{filename_orig}/{dset}"

                        # Check for wildcard match with existing keys
                        found = False
                        for existing_key in list(workflow_links.keys()):
                            if _match(existing_key, full_path_orig) and not found:
                                node_idx = len(workflow.nodes)
                                if node_idx == 0:
                                    workflow_links[existing_key].append(999)
                                else:
                                    workflow_links[existing_key].append(node_idx)
                                found = True

                        if not found:
                            node_idx = len(workflow.nodes)
                            if node_idx == 0:
                                workflow_links[full_path_orig] = [999]
                            else:
                                if full_path_orig not in workflow_links:
                                    workflow_links[full_path_orig] = []
                                workflow_links[full_path_orig].append(node_idx)

                        # Default values: p->0, m->1
                        passthru = dset_yaml.get("passthru", 0)
                        metadata = dset_yaml.get("metadata", 1)

                        if not (metadata + passthru):
                            print(
                                "Error: Either metadata or passthru must be enabled. "
                                "Both cannot be disabled.",
                                file=sys.stderr,
                            )
                            sys.exit(1)

                        l5_port = LowFivePort(
                            name=full_path,
                            filename=filename,
                            dset=dset,
                            passthru=passthru,
                            metadata=metadata,
                            io_freq=io_freq,
                        )
                        node.l5_inports.append(l5_port)

            # -- Parse outports --
            if "outports" in task_yaml:
                for outport_yaml in task_yaml["outports"]:
                    filename = outport_yaml["filename"]
                    filename_orig = filename

                    if "range" in outport_yaml:
                        file_range = outport_yaml["range"]

                    # Apply ensemble filename convention
                    if task_count > 1:
                        dot_pos = filename.find(".")
                        if dot_pos != -1:
                            pre_dlm = filename[:dot_pos]
                            post_dlm = filename[dot_pos:]
                        else:
                            pre_dlm = filename
                            post_dlm = ""
                        filename = f"{pre_dlm}-inst{file_range[index]}{post_dlm}"

                    # Parse datasets
                    for dset_yaml in outport_yaml["dsets"]:
                        dset = dset_yaml["name"]
                        full_path = f"{filename}/{dset}"
                        full_path_orig = f"{filename_orig}/{dset}"

                        # Default values: o->0, p->0, m->1
                        zerocopy = dset_yaml.get("zerocopy", 0)
                        passthru = dset_yaml.get("passthru", 0)
                        metadata = dset_yaml.get("metadata", 1)

                        # Check for wildcard match with existing keys
                        found = False
                        for existing_key in list(workflow_links.keys()):
                            if _match(existing_key, full_path_orig) and not found:
                                workflow_links[existing_key].append(-len(workflow.nodes))
                                found = True

                        if not found:
                            if full_path_orig not in workflow_links:
                                workflow_links[full_path_orig] = []
                            # consumers have positive, producers have negative values
                            workflow_links[full_path_orig].append(-len(workflow.nodes))

                        if not (metadata + passthru):
                            print(
                                "Error: Either metadata or passthru must be enabled. "
                                "Both cannot be disabled.",
                                file=sys.stderr,
                            )
                            sys.exit(1)

                        l5_port = LowFivePort(
                            name=full_path,
                            filename=filename,
                            dset=dset,
                            zerocopy=zerocopy,
                            passthru=passthru,
                            metadata=metadata,
                            io_freq=1,
                        )
                        node.l5_outports.append(l5_port)

            workflow.nodes.append(node)

    # -- Generate links --
    l = 0
    for k, v in workflow_links.items():
        idx_prod = []
        idx_con = []
        for val in v:
            if val > 0:
                if val == 999:
                    idx_con.append(0)
                else:
                    idx_con.append(val)
            else:
                idx_prod.append(val)

        # Skip generating links for orphan ports
        if len(idx_prod) == 0 or len(idx_con) == 0:
            if len(idx_prod) == 0:
                # Producer reading input file from disk (no producer in the graph)
                for idx in idx_con:
                    for in_port in workflow.nodes[idx].l5_inports:
                        if _match(k, in_port.name):
                            fname = in_port.filename
                            dset = in_port.dset
                            if in_port.metadata:
                                print(
                                    f"ERROR: No matching link found for the inport "
                                    f"{fname}{dset} requesting memory mode. "
                                    f"Please use the file mode or specify a matching outport.",
                                    file=sys.stderr,
                                )
                                sys.exit(1)
                            workflow.nodes[idx].passthru_files.append((fname, dset))
            else:
                # Consumer writing output file to disk (no consumer in the graph)
                for idx in idx_prod:
                    for out_port in workflow.nodes[-idx].l5_outports:
                        if _match(k, out_port.name):
                            fname = out_port.filename
                            dset = out_port.dset
                            if out_port.metadata:
                                print(
                                    f"ERROR: No matching link found for the outport "
                                    f"{fname}{dset} requesting memory mode. "
                                    f"Please use the file mode or specify a matching inport.",
                                    file=sys.stderr,
                                )
                                sys.exit(1)
                            workflow.nodes[-idx].passthru_files.append((fname, dset))
        else:
            # Generate links in round-robin fashion
            if len(idx_prod) <= len(idx_con):
                quot = len(idx_con) // len(idx_prod)
                l = _generate_links(idx_con, idx_prod, quot, workflow, k, l, 0)
            else:
                quot = len(idx_prod) // len(idx_con)
                l = _generate_links(idx_prod, idx_con, quot, workflow, k, l, 1)
