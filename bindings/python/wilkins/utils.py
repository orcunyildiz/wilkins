#orc@25-01: adding support for defining callback actions externally via YAML file
def import_from(module, name):
    from importlib.util import find_spec
    if not find_spec(module):
        print('%s: No such file exists for the callback actions.' % module, file=sys.stderr)
        exit(1)
    module = __import__(module, fromlist=[name])
    return getattr(module, name)

def exec_stateful(puppets, myTasks, vol, wlk_consumer, pm, nm):
    import pyhenson as h
    substitute_fn=-1
    task_args = puppets[myTasks[0]][1]
    for i in range(len(task_args)):
        if '{filename}' in task_args[i]:
            substitute_fn = i

    myPuppet = h.Puppet(puppets[myTasks[0]][0], task_args, pm, nm)

    #support for dynamic filenames
    if substitute_fn!=-1:
        def scf_cb():
            fnames = vol.get_filenames(wlk_consumer)
            print(f"{fnames = }")
            return fnames[0] #TODO: We can also return the latest one if we want.
        vol.set_consumer_filename(scf_cb)

    #TODO: passthru support
    #wilkins.wait()
    myPuppet.proceed()
    #wilkins.commit()

def exec_stateless(puppets, myTasks, vol, wlk_consumer, wlk_producer, pm, nm):
    import pyhenson as h
    from collections import defaultdict
    substitute_fn = -1
    task_args = puppets[myTasks[0]][1]
    for i in range(len(task_args)):
        if '{filename}' in task_args[i]:
            substitute_fn = i

    if substitute_fn==-1: #static arg list, create the puppet only once
        myPuppet = h.Puppet(puppets[myTasks[0]][0], task_args, pm, nm)

    #TODO: need to take into account the if io_proc==1:
    #TODO: passthru support.
    if wlk_consumer!=-1:
        while True:
            fnames = []
            fnames = vol.get_filenames(wlk_consumer)
            print(f"{fnames = }")
            if fnames:
                for i in range(len(fnames)):
                    if substitute_fn!=-1: #orc@01-05: user requested dynamic filenames
                        def scf_cb():
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
            else:
                vol.send_done(wlk_consumer)
                break
    if wlk_producer==1:
        myPuppet.proceed()
        vol.producer_done()
