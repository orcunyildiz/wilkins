#Usage: mpirun -n $nprocs python wilkins-master.py config_file
#config_files= [wilkins_prod_con.yaml, wilkins_prod_2cons.yaml]

import os

#NB: Assuming that user has these env set either i)after spack installation or ii)sourcing vol-plugin.sh explicitly
#os.environ["HDF5_PLUGIN_PATH"] = "/Users/oyildiz/Work/software/lowfive/build/src"
#os.environ["HDF5_VOL_CONNECTOR"] = "lowfive under_vol=0;under_info={};"

if not os.path.exists(os.path.join(os.environ["HDF5_PLUGIN_PATH"], "liblowfive.so")): #liblowfive.dylib #liblowfive.so
    raise RuntimeError("Bad HDF5_PLUGIN_PATH")

import pyhenson as h
import sys

from mpi4py import MPI

import lowfive

#TODO: omit print statements
serve_counter = 0
def flow_control(vol, comm, intercomms, serve_indices, flowPolicies):
    def bsa_cb():
        comm.barrier()
        global serve_counter
        import sys
        indices = []
        for m in range(len(serve_indices)):
            indices.append(m)
        serve = 0
        serve_counter = serve_counter + 1
        for fp in flowPolicies:
            N   = fp[0]
            idx = fp[1]
            if N==-1: #latest
                print("Latest policy for index ", idx)
                probe = intercomms[idx].iprobe(tag=2) #checking whether there are any msgs from consumer
                if probe:
                    serve = 1
                global_serve = comm.allreduce(serve, op=MPI.MAX)
                if global_serve:
                    print("LATEST:serving now: Got msg from con")
                else:
                    print("LATEST:skipping serving: no msg from con")
                    indices.remove(idx)
            else:
       	       	print("Some policy for index ", idx)
                if serve_counter % N != 0:
                    indices.remove(idx)
                    print("SOME:skipping serving")
                else:
                    print("SOME:serving now")
        print("selected intercomms at counter", serve_counter, ": ", indices)
        return indices

    vol.set_serve_indices(bsa_cb)

from wilkins.utils import*

#orc@22-11: adding the wlk py bindings
from wilkins import pywilkins as w

world = MPI.COMM_WORLD.Dup()
size = world.Get_size()
rank = world.Get_rank()

#orc@22-11: generating procmap via YAML
config_file = sys.argv[1]
workflow = w.Workflow()
workflow.make_wflow_from_yaml(config_file)
procs_yaml = []
myTasks    = []
puppets    = []
actions    = []
passthru_files = []
i = 0
ensembles = 0
for node in workflow.nodes:
    procs_yaml.append((node.func, node.nprocs))
    task_exec = node.func #"./" + node.func  + ".hx"; #TODO: This can be an extra field in the YAML file if needed.
    puppets.append((task_exec, node.args))
    if node.actions:
        actions.append((node.func, node.actions))
    if node.taskCount > 1:
        ensembles = 1
    #bookkeping of tasks belonging to the execution group
    if rank>=node.start_proc and rank < node.start_proc + node.nprocs:
        myTasks.append(i)
        passthru_files = node.passthru_files
    i = i+1

pm = h.ProcMap(world, procs_yaml)
nm = h.NameMap()

a = MPI._addressof(MPI.COMM_WORLD)
wilkins = w.Wilkins(a,config_file)

lowfive.create_logger("info") #trace #info

#orc@31-03: adding for the new control logic: consumer looping until there are files
wlk_producer = -1
wlk_consumer = [] #orc@25-07: making this an array for fanin cases as same consumer connected to multiple producer instances
vol = None
pl_prod     = []
pl_con      = []
serve_indices = []
#NB: In some cases, L5 comms should only include subset of processes (e.g., rank 0 from LPS)
io_proc = wilkins.is_io_proc()
if io_proc==1:
    comm = w.get_local_comm(wilkins)
    local_rank = comm.Get_rank()
    intercomms = w.get_intercomms(wilkins)
    vol = lowfive.create_DistMetadataVOL(comm, intercomms)
    l5_props = wilkins.set_lowfive()
    execGroup         = []
    set_si = 0
    from collections import defaultdict
    flowPolicies = defaultdict(list) #key: prodName value: (flowPolicy, intercomm index)
    passthruList = defaultdict(list) #key: execGroup value: (prodIndex, conIndex) #orc@09-06: added for passthru support
    flow_execGroup    = []
    #support for reading/writing files from/to disk (without matching links)
    for pf in passthru_files:
        vol.set_passthru(pf[0], pf[1])
    for prop in l5_props:
        #print(prop.filename, prop.dset, prop.producer, prop.consumer, prop.execGroup, prop.memory, prop.prodIndex, prop.conIndex, prop.zerocopy, prop.flowPolicy)
        if prop.memory==1: #TODO: L5 doesn't support both memory and passthru at the moment. This logic might change once L5 supports both modes.
            vol.set_memory(prop.filename, prop.dset)
        else:
            vol.set_passthru(prop.filename, prop.dset)
            #orc@09-06: constructing the passthru list
            if not passthruList.get(prop.execGroup):
                passthruList[prop.execGroup].append((prop.prodIndex, prop.conIndex))
        if prop.consumer==1 and not any(x in prop.execGroup for x in execGroup): #orc: setting single intercomm per execGroup.
            if ensembles!=1:
                vol.set_intercomm(prop.filename, prop.dset, prop.conIndex)
            wlk_consumer.append(prop.conIndex)
            execGroup.append(prop.execGroup)
        if prop.producer==1: #NB: Task can be both producer and consumer.
            wlk_producer = 1
            if prop.prodIndex not in serve_indices:
                serve_indices.append(prop.prodIndex)
            if prop.zerocopy==1:
                vol.set_zerocopy(prop.filename, prop.dset)
        #adding flow control logic
        if prop.producer==1 and prop.flowPolicy!=1 and not any(x in prop.execGroup for x in flow_execGroup): #setting single flow control policy per execGroup
            prodName = prop.execGroup.split(":")[0]
            flow_execGroup.append(prop.execGroup)
            flowPolicies[prodName].append((prop.flowPolicy, prop.prodIndex))

    def bsa_cb():
        return serve_indices

    #if any flow control policies, handling them here.
    for fp in flowPolicies:
        if wilkins.my_node(fp):
            flow_control(vol, comm, intercomms, serve_indices, flowPolicies.get(fp))
            set_si = 1

    if not set_si: #NB: set serve_indices if not set within the flow control
        vol.set_serve_indices(bsa_cb)

    #if any cb actions, setting them here.
    for action in actions:
        if wilkins.my_node(action[0]):
            file_name = action[1][0]
            cb_func   = action[1][1]
            cb =  import_from(file_name, cb_func) 
            cb(vol, local_rank)  #NB: For more advanced callbacks with args, users would need to write their own wilkins.py 

    #orc@09-06: determining the mode.
    pl_prod, pl_con = get_passthru_lists(wilkins, passthruList)

onlinePassthru = False
if '-o' in sys.argv:
    onlinePassthru = True
    sys.argv.remove('-o')
    print("Providing onlinePassthru support")

exec_task(wilkins, puppets, myTasks, vol, wlk_consumer, wlk_producer, pl_prod, pl_con, pm, nm, io_proc, ensembles, serve_indices, onlinePassthru)

#orc: deprecated as using exec_task to run both stateful&stateless tasks.
#TODO: Add the logic for TP mode when L5 supports it (simply iterate thru myTasks)
#stateful = False

#if '-s' in sys.argv:
#    stateful = True
#    sys.argv.remove('-s')
#    print("Running stateful consumer")
#else:
#    print("Nothing specified. Running stateless consumer")

#if stateful:
#    exec_stateful(puppets, myTasks, vol, wlk_consumer, wlk_producer, pl_prod, pl_con, pm, nm, io_proc, ensembles, serve_indices)
#else:
#    exec_stateless(puppets, myTasks, vol, wlk_consumer, wlk_producer, pl_prod, pl_con, pm, nm, ensembles)
