import os

#orc: user would need to source the vol-plugin.sh
os.environ["HDF5_PLUGIN_PATH"] = "/lcrc/project/PEDAL/oyildiz/outdated-repos-backup/lowfive-101122/build/src"
os.environ["HDF5_VOL_CONNECTOR"] = "lowfive under_vol=0;under_info={};"
os.environ["LD_LIBRARY_PATH"] += (":" + os.environ["HDF5_PLUGIN_PATH"])

if not os.path.exists(os.path.join(os.environ["HDF5_PLUGIN_PATH"], "liblowfive.so")):
    raise RuntimeError("Bad HDF5_PLUGIN_PATH")


import pyhenson as h
import sys

from mpi4py import MPI

import lowfive

#orc@25-01: adding support for defining callback actions externally via YAML file
def import_from(module, name):
    from importlib.util import find_spec
    if not find_spec(module):
        print('%s: No such file exists for the callback actions.' % module, file=sys.stderr)
        exit(1)
    module = __import__(module, fromlist=[name])
    return getattr(module, name)

#orc@22-11: adding the wlk py bindings
import pywilkins as w

world = MPI.COMM_WORLD.Dup()
size = world.Get_size()
rank = world.Get_rank()

#orc@22-11: generating procmap via YAML
workflow = w.Workflow()
workflow.make_wflow_from_yaml("wilkins_freeze_detect.yaml")
procs_yaml = []
myTasks    = []
puppets    = []
actions    = []
i = 0
for node in workflow.nodes:
    procs_yaml.append((node.func, node.nprocs))
    task_exec = "./" + node.func  + ".hx"; #TODO: This can be an extra field in the YAML file if needed.
    puppets.append((task_exec, node.args))
    if node.actions:
        actions.append((node.func, node.actions))
    #bookkeping of tasks belonging to the execution group
    if rank>=node.start_proc and rank < node.start_proc + node.nprocs:
        myTasks.append(i)
    i = i+1

pm = h.ProcMap(world, procs_yaml)
nm = h.NameMap()

a = MPI._addressof(MPI.COMM_WORLD)
wilkins = w.Wilkins(a,"wilkins_freeze_detect.yaml")

#NB: In some cases, L5 comms should only include subset of processes (e.g., rank 0 from LPS)
io_proc = wilkins.is_io_proc()
if io_proc==1:
    comm = w.get_local_comm(wilkins)
    intercomms = w.get_intercomms(wilkins)
    vol = lowfive.DistMetadataVOL(comm, intercomms)
    l5_props = wilkins.set_lowfive()
    execGroup = []
    for prop in l5_props:
        print(prop.filename, prop.dset, prop.producer, prop.consumer, prop.execGroup, prop.memory, prop.index, prop.zerocopy)
        if prop.memory==1: #TODO: L5 doesn't support both memory and passthru at the moment. This logic might change once L5 supports both modes.
            vol.set_memory(prop.filename, prop.dset)
        else:
            vol.set_passthru(prop.filename, prop.dset)
        if prop.consumer==1 and not any(x in prop.execGroup for x in execGroup): #orc: setting single intercomm per execGroup.
            vol.set_intercomm(prop.filename, prop.dset, prop.index)
            execGroup.append(prop.execGroup)
        if prop.producer==1 and prop.zerocopy==1: #NB: Task can be both producer and consumer.
            vol.set_zerocopy(prop.filename, prop.dset)

    #if any cb actions, setting them here.
    for action in actions:
        if wilkins.my_node(action[0]):
            file_name = action[1][0]
            cb_func   = action[1][1]
            cb =  import_from(file_name, cb_func) 
            cb(vol)  #NB: For more advanced callbacks with args, users would need to write their own wilkins.py 

#TODO: Add the logic for TP mode when L5 supports it (simply iterate thru myTasks)
#print(puppets[myTasks[0]])
myPuppet = h.Puppet(puppets[myTasks[0]][0], puppets[myTasks[0]][1], pm, nm)

wilkins.wait() #consumer waits until the data is ready (i.e., producer's commit function) (if producer or memory mode: void)
myPuppet.proceed()
wilkins.commit() #producer signals that data is ready to use (if consumer or memory mode: void)


