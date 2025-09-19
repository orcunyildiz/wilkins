import sys
import os
import time
import importlib
import fnmatch

import pyhenson as h

def get_source_index(pl_con, target_filename):
    if target_filename in pl_con:
        return pl_con[target_filename]

    # wildcard matching
    for pattern, con_index in pl_con.items():
        if fnmatch.fnmatch(target_filename, pattern):
            return con_index

    return None

# TODO use the set_consumer_filename instead of before_file_open
#once L5 cb has the name argument, and omit filename_cache
filename_cache = {}
def setup_passthru_callbacks(vol, role, pl_con=[], keep=True):
    if role == "producer":

        def after_file_close(name):
            vol.serve_all(True, False)

        vol.set_send_filename(after_file_close)
        vol.set_keep(keep)

    elif role == "consumer":

        def before_file_open(name):
            source_index = get_source_index(pl_con, name)

            if source_index is None:
                available_patterns = list(pl_con.keys())
                raise ValueError(
                    f"No matching consumer found for filename '{name}'. "
                    f"Available patterns: {available_patterns}"
                )
            filenames = vol.get_filenames(source_index)
            filename_cache[source_index] = filenames
            vol.send_done(source_index)

        vol.set_before_file_open(before_file_open)


# defining callback actions externally via YAML file
def import_from(module, name):
    from importlib.util import find_spec

    if not find_spec(module):
        print(
            f"{module}: No such file exists for the callback actions.", file=sys.stderr
        )
        exit(1)
    module = __import__(module, fromlist=[name])
    return getattr(module, name)


def get_script_name(exec_name):

    normalized_path = os.path.abspath(exec_name)                                                                                                                       
    if not os.path.exists(normalized_path):                                                                                                                            
        raise FileNotFoundError(f"Script not found: {exec_name}")                                                                                                      
    script_dir = os.path.dirname(normalized_path)                                                                                                                      
    script_filename = os.path.basename(normalized_path)
    if script_filename.endswith('.py'):                                                                                                                                
        script_name = script_filename[:-3]                                                                                                                             
    else:                                                                                                                                                              
        script_name = script_filename                                                                                                                                  
        normalized_path += '.py'                                                                                                                                       
    if script_dir not in sys.path:                                                                                                                                     
        sys.path.insert(0, script_dir)    
    return script_name


def get_passthru_lists(wilkins, passthru_list):

    pl_send = []
    pl_recv = {}
    for pl in passthru_list:
        prod_name = pl.split(":")[0]
        con_name = pl.split(":")[1]
        pl_val = passthru_list.get(pl)
        if wilkins.my_node(prod_name):
            pl_send.append(pl_val[0][0])
        if wilkins.my_node(con_name):  # pl_val[i][0]: prodIndex, pl_val[i][1]: conIndex, pl_val[i][2]: filename
            pl_recv[pl_val[0][2]] = pl_val[0][1]

    return pl_send, pl_recv


idx = 0


def exec_task(
    wilkins,
    puppets,
    my_tasks,
    vol,
    wlk_consumer,
    wlk_producer,
    pl_prod,
    pl_con,
    pm,
    nm,
    io_proc,
    ensembles,
    serve_indices,
    single_iter_passthru,
):

    substitute_fn = -1
    task_args = puppets[my_tasks[0]][1]
    for i in range(len(task_args)):
        if "{filename}" in task_args[i]:
            substitute_fn = i

    my_puppet = None
    python_puppet = False
    exec_name = puppets[my_tasks[0]][0]
    if exec_name.endswith(".py"):
        python_puppet = True
    else:
        my_puppet = h.Puppet(puppets[my_tasks[0]][0], task_args, pm, nm)

    if wlk_consumer:

        def scf_cb():
            global idx
            if ensembles:
                # NB: this is again in fanout, fast consumer might fetch the old file name from prev iter in multiple iters
                # you run into problems at serve_all in producer
                time.sleep(3)
            if pl_con and wlk_consumer[idx] in filename_cache:
                fnames = filename_cache[wlk_consumer[idx]]
                print(f"{fnames = } (from cache)")
            else:
                fnames = vol.get_filenames(wlk_consumer[idx])
                print(f"{fnames = } (from vol)")
            if ensembles:
                vol.set_intercomm(fnames[0], "*", wlk_consumer[idx])
                idx = idx + 1
                if idx == len(wlk_consumer):
                    idx = 0
            return fnames[-1]

        if substitute_fn != -1 and (
            not single_iter_passthru
        ):  # support for dynamic filenames
            vol.set_consumer_filename(scf_cb)

    if single_iter_passthru:
        wilkins.wait()

    if python_puppet:
        script_name = get_script_name(exec_name)
        py_script = importlib.import_module(script_name)
        if hasattr(py_script, "main") and callable(py_script.main):
            try:
                py_script.main(task_args)
            except TypeError:
                py_script.main()
    else:
        my_puppet.proceed()

    if single_iter_passthru:
        wilkins.commit()

    # setting serve_indices again for the producer_done (which could be set before in flow_control causing problems)
    def bsa_cb():
        return serve_indices

    if io_proc == 1:
        vol.set_serve_indices(bsa_cb)

    # this is to resolve deadlock in a cycle where there are multiple tasks as both producers and consumers
    prod_first = 0
    prod_done = 0
    if wlk_producer == 1 and wlk_consumer:
        # Cycle topology in a pipeline fashion (e.g., 0->1->2->0)
        if len(serve_indices) == 1 and len(wlk_consumer) == 1:
            if serve_indices[0] > wlk_consumer[0]:
                prod_first = 1
        # Cycle topology where task 0 connects with multiple tasks
        elif len(serve_indices) > 1 and len(wlk_consumer) > 1:
            if serve_indices[0] < wlk_consumer[0]:
                prod_first = 1

    if wlk_producer == 1 and io_proc == 1:
        if wlk_consumer:
            if prod_first:
                vol.producer_done()
                prod_done = 1
        else:
            vol.producer_done()
            prod_done = 1

    if wlk_consumer:
        while wlk_consumer:
            for con_idx in wlk_consumer:
                fnames = []
                if ensembles:
                    # There might be files in the producer still consuming by the other consumer tasks in the fan-out scenario
                    time.sleep(3)  # to resolve the race condition in fan-out topology.
                fnames = vol.get_filenames(con_idx)
                print(f"{fnames = }")
                if fnames:  # more data to consume
                    if python_puppet:
                        try:
                            py_script.main(task_args)
                        except TypeError:
                            py_script.main()
                    else:
                        my_puppet.proceed()
                else:
                    vol.send_done(con_idx)
                    wlk_consumer.remove(con_idx)

    # if producer hasn't issued a done signal yet (for cycle topology)
    if wlk_producer == 1 and not prod_done:
        vol.producer_done()
