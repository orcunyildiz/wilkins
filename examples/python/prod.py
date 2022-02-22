import pywilkins as w
from mpi4py import MPI
import sys
import time

#create the wilkins object
a = MPI._addressof(MPI.COMM_WORLD)
r = MPI.COMM_WORLD.Get_rank()
wilkins = w.Wilkins(a,"wilkins_prod_con.yaml")

#local communicator of the task
comm = w.get_local_comm(wilkins)
size = comm.Get_size()
rank = comm.Get_rank()
print("producer rank is " + str(rank) + " within size of " + str(size))

#intercomms of the task
#intercomms = w.get_intercomms(wilkins) #NB: collective call, needs to be called in both sides of the prod-con pairs.
#for ic in intercomms:
#    remote_size = ic.Get_remote_size()
#    print("producer " + str(r) + ": remote group size is " + str(remote_size))

sys.stdout.flush()

wilkins.initStandalone()

#some computation and data generation should be here...
time.sleep(3)
#...
#...

wilkins.commit() #producer signals that data is ready to use

print("producer " + str(r) + " terminating")
