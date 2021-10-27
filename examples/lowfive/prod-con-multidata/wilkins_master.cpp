#include    <thread>

#include    <dlfcn.h>

#include <diy/mpi/communicator.hpp>
using communicator = diy::mpi::communicator;

#include <wilkins/wilkins.hpp>
#include <wilkins/context.h>

using namespace wilkins;

int main(int argc, char* argv[])
{

    diy::mpi::environment     env(argc, argv, MPI_THREAD_MULTIPLE);
    diy::mpi::communicator    world;

    Workflow workflow;
    std::string config_file = argv[1];
    Workflow::make_wflow_from_yaml(workflow, config_file);

    Wilkins* wilkins = new Wilkins(MPI_COMM_WORLD, config_file);

    std::vector<void*> vec_dlsym;
    std::vector<int> myTasks;

    communicator local = wilkins->local_comm_handle();
    communicator local_dup;
    local_dup.duplicate(local); //orc@27-10: we might need to duplicate intercomms as well later for TP or we might need to use world duplicates for both

    std::vector<communicator> intercomms = wilkins->build_intercomms();
    //fmt::print("MASTER: intercomm.size() = {}\n",intercomms[0].size());

    for (size_t i = 0; i < workflow.nodes.size(); i++)
    {
        std::string task_exec    = "./" + workflow.nodes[i].func  + ".so";
        int start_proc           = workflow.nodes[i].start_proc;
        int nprocs               = workflow.nodes[i].nprocs;

        if (world.rank() >= start_proc &&  world.rank() < start_proc + nprocs)
        {
            //keeping track of which tasks to run
            myTasks.emplace_back(i);
        }

        //load tasks
        void* lib_task = dlopen(task_exec.c_str(), RTLD_LAZY);
        if (!lib_task)
            fmt::print(stderr, "Couldn't open {}\n", task_exec.c_str());

        void* task_main_ = dlsym(lib_task, "main");
        if (!task_main_)
            fmt::print(stderr, "Couldn't find main at task {}\n",task_exec.c_str());

        void* task_intercomm_ = dlsym(lib_task, "wilkins_set_intercomms");
        if (!task_intercomm_)
            fmt::print(stderr, "Couldn't find task set_intercomms at task {}\n",task_exec.c_str());

        void* task_local_comm_ = dlsym(lib_task, "wilkins_set_local_comm");
        if (!task_local_comm_)
            fmt::print(stderr, "Couldn't find task set_local_comm at task {}\n",task_exec.c_str());

        vec_dlsym.emplace_back(task_main_);
        vec_dlsym.emplace_back(task_intercomm_);
        vec_dlsym.emplace_back(task_local_comm_);
    }

    if (myTasks.size() > 1)
    {
        fmt::print("shared mode with {} tasks sharing the same resources\n", myTasks.size());
        //TODO TP mode
    }
    else if (myTasks.size() == 1)
    {
        //SP mode
        auto task_main = [&]()
        {
            ((void (*) (int, char**)) (vec_dlsym[(3 * myTasks[0])]))(argc, argv);
        };

        auto task_intercomm = [&]()
        {
            ((void (*) (void*)) (vec_dlsym[(3*myTasks[0])+1]))(&intercomms);
        };

        auto task_local_comm = [&]()
        {
            ((void (*) (void*)) (vec_dlsym[(3*myTasks[0])+2]))(&local_dup);
        };

        task_intercomm();
	task_local_comm();
        task_main();

    }
    else
    {
     	fmt::print(stderr, "ERROR: Wrong workflow graph configuration. Ranks cannot have {} tasks assigned.\n", myTasks.size());
        exit(1);
    }

    //TODO: cont'd with the shared

/*
    std::mutex exclusive;

    if (!shared)
    {
        if (producer)
            producer_f();
        else
            consumer_f();
    } else
    {
        auto producer_thread = std::thread(producer_f);
        auto consumer_thread = std::thread(consumer_f);

        producer_thread.join();
        consumer_thread.join();
    }

*/
}
