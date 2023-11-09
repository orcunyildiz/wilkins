#orc@25-01: adding support for defining callback actions externally via YAML file
def import_from(module, name):
    from importlib.util import find_spec
    if not find_spec(module):
        print('%s: No such file exists for the callback actions.' % module, file=sys.stderr)
        exit(1)
    module = __import__(module, fromlist=[name])
    return getattr(module, name)

def get_passthru_lists(wilkins, passthruList):

    pl_send = []
    pl_recv  = []
    for pl in passthruList:
        prodName = pl.split(":")[0]
        conName = pl.split(":")[1]
        pl_val = passthruList.get(pl)
        if wilkins.my_node(prodName):
            pl_send.append(pl_val[0][0])
        if wilkins.my_node(conName): #pl_val[i][0]: prodIndex, pl_val[i][1]: conIndex
            pl_recv.append(pl_val[0][1])

    return pl_send, pl_recv

idx = 0
def exec_task(puppets, myTasks, vol, wlk_consumer, wlk_producer, pl_prod, pl_con, pm, nm, io_proc, ensembles, serve_indices):
    import pyhenson as h
    substitute_fn=-1
    task_args = puppets[myTasks[0]][1]
    for i in range(len(task_args)):
        if '{filename}' in task_args[i]:
            substitute_fn = i

    myPuppet = h.Puppet(puppets[myTasks[0]][0], task_args, pm, nm)

    if wlk_consumer:
        def scf_cb():
            global idx
            if ensembles:
                import time  #TODO: think on a better workaround for fanout.
                time.sleep(3) #NB: this is again in fanout, fast consumer might fetch the old file name from prev iter in multiple iters #you run into problems at serve_all in producer
            fnames = vol.get_filenames(wlk_consumer[idx])
            if wlk_consumer[idx] in pl_con: #passthru requires extra signaling for prod to exit from serving
                vol.send_done(wlk_consumer[idx])
            print(f"{fnames = }")
            if ensembles:
                vol.set_intercomm(fnames[0], "*", wlk_consumer[idx])
                idx = idx + 1
                if idx==len(wlk_consumer):
                    idx = 0
            return fnames[0] #TODO: We can also return the latest one if we want.        

        if substitute_fn!=-1 or pl_con: #support for dynamic filenames or in passthru mode this works as a barrier
            vol.set_consumer_filename(scf_cb)

    if wlk_producer==1 and io_proc==1:
        def afc_cb():
            vol.serve_all(True, False)
            vol.clear_files() #since keep set to True, need to clear files manually
        if pl_prod: #if passthru, issue serve_all for get_filename to work at the consumer
            vol.set_keep(True)
            vol.set_after_file_close(afc_cb)

    myPuppet.proceed()

    #setting serve_indices again for the producer_done (which could be set before in flow_control causing problems) 
    def bsa_cb():
        return serve_indices

    vol.set_serve_indices(bsa_cb)

    #this is to resolve deadlock in a cycle where there are multiple tasks as both producers and consumers
    prodFirst = 0
    prodDone = 0
    if wlk_producer==1 and wlk_consumer:
        if serve_indices[0] < wlk_consumer[0]:
            prodFirst = 1

    if wlk_producer==1 and io_proc==1:
        if wlk_consumer:
            if prodFirst:
                vol.producer_done()
                prodDone = 1
        else:
            vol.producer_done()
            prodDone = 1

    if wlk_consumer:
        while wlk_consumer:
            for con_idx in wlk_consumer:
                fnames = []
                if ensembles: #There might be files in the producer still consuming by the other consumer tasks in the fan-out scenario
                    import time
                    time.sleep(3) #to resolve the race condition in fan-out topology.
                fnames = vol.get_filenames(con_idx)
                print(f"{fnames = }")
                if fnames: #more data to consume
                   myPuppet.proceed()
                else:
                   vol.send_done(con_idx)
                   wlk_consumer.remove(con_idx)

    #if producer hasn't issued a done signal yet (for cycle topology)
    if wlk_producer==1 and not prodDone:
        vol.producer_done()

#deprecated
def exec_stateful(puppets, myTasks, vol, wlk_consumer, wlk_producer, pl_prod, pl_con, pm, nm, io_proc, ensembles):
    import pyhenson as h
    print("Warning: Using a deprecated function to run stateful consumers.")
    substitute_fn=-1
    task_args = puppets[myTasks[0]][1]
    for i in range(len(task_args)):
        if '{filename}' in task_args[i]:
            substitute_fn = i

    myPuppet = h.Puppet(puppets[myTasks[0]][0], task_args, pm, nm)

    if wlk_consumer:
        def scf_cb():
            global idx
            fnames = vol.get_filenames(wlk_consumer[idx])
            if wlk_consumer[idx] in pl_con: #passthru requires extra signaling for prod to exit from serving
                vol.send_done(wlk_consumer[idx])
            print(f"{fnames = }")
            if ensembles:
                vol.set_intercomm(fnames[0], "*", wlk_consumer[idx])
                idx = idx + 1
                if idx==len(wlk_consumer):
                    idx = 0
            return fnames[0] #TODO: We can also return the latest one if we want.        

        if substitute_fn!=-1 or pl_con: #support for dynamic filenames or in passthru mode this works as a barrier
            vol.set_consumer_filename(scf_cb)

    if wlk_producer==1 and io_proc==1:
        def afc_cb():
            vol.serve_all(True, False)
            vol.clear_files() #since keep set to True, need to clear files manually
        if pl_prod: #if passthru, issue serve_all for get_filename to work at the consumer
            vol.set_keep(True)
            vol.set_after_file_close(afc_cb)

    myPuppet.proceed()

#deprecated
def exec_stateless(puppets, myTasks, vol, wlk_consumer, wlk_producer, pl_prod, pl_con, pm, nm, ensembles):
    import pyhenson as h
    from collections import defaultdict
    print("Warning: Using a deprecated function to run stateless consumers.")
    substitute_fn = -1
    task_args = puppets[myTasks[0]][1]
    for i in range(len(task_args)):
        if '{filename}' in task_args[i]:
            substitute_fn = i

    if substitute_fn==-1: #static arg list, create the puppet only once
        myPuppet = h.Puppet(puppets[myTasks[0]][0], task_args, pm, nm)

    #TODO: need to take into account the if io_proc==1:
    if wlk_consumer:
        while wlk_consumer:
            for con_idx in wlk_consumer:
                fnames = []
                fnames = vol.get_filenames(con_idx)
                print(f"{fnames = }")
                if fnames:
                    for i in range(len(fnames)):
                        if substitute_fn!=-1: #orc@01-05: user requested dynamic filenames
                            def scf_cb():
                                if ensembles:
                                    vol.set_intercomm(fnames[i], "*", con_idx)
                                return fnames[i] #TODO: We can also return the latest one if we want.
                            vol.set_consumer_filename(scf_cb)
                            fname = fnames[i]
                            args_str = ' '.join(task_args)
                            #NB: We can extend arg substitution for other args than filename as well (e.g., timesteps, with the dict user provides)
                            args_str = args_str.format_map(defaultdict(str,filename=fname))
                            curr_task_args = args_str.split()
                            myPuppet = h.Puppet(puppets[myTasks[0]][0], curr_task_args, pm, nm)
                        #NB:run this in a for loop for each filename. Our flow control is orthogonal, user can skip some/latest iters.
                        #However, granularity within one iteration could be multiple files.
                        myPuppet.proceed()
                        if con_idx in pl_con: #passthru requires extra signaling
                            vol.send_done(con_idx)
                else:
                    vol.send_done(con_idx)
                    wlk_consumer.remove(con_idx)

    if wlk_producer==1:
        def afc_cb():
            vol.serve_all(True, False)
            vol.clear_files() #since keep set to True, need to clear files manually
        if pl_prod:  #if passthru, issue serve_all for get_filename to work at the consumer
            vol.set_keep(True)
            vol.set_after_file_close(afc_cb)
        myPuppet.proceed()
        vol.producer_done()
