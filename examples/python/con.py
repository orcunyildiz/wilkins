import pywilkins as w
from mpi4py import MPI

a = MPI._addressof(MPI.COMM_WORLD)
r = MPI.COMM_WORLD.Get_rank()
wilkins = w.Wilkins(a,"wilkins_prod_con.yaml")

comm = w.get_local_comm(wilkins)
size = comm.Get_size()
rank = comm.Get_rank()
print("consumer rank is " + str(rank) + " within size of " + str(size))

#intercomms = w.get_intercomms(wilkins) #NB: collective call, needs to be called in both sides of the prod-con pairs.
#for ic in intercomms:
#    remote_size = ic.Get_remote_size()
#    print("consumer " + str(r) + ": remote group size is " + str(remote_size))

print("consumer " + str(r) + " terminating")
