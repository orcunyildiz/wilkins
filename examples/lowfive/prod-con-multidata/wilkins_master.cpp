#include    <thread>

#include    <dlfcn.h>

#include <diy/mpi/communicator.hpp>
using communicator = diy::mpi::communicator;

#include <wilkins/wilkins.hpp>

using namespace wilkins;

int main(int argc, char* argv[])
{

    diy::mpi::environment     env(argc, argv, MPI_THREAD_MULTIPLE);
    diy::mpi::communicator    world;

    Workflow workflow;
    std::string config_file = argv[1];
    Workflow::make_wflow_from_yaml(workflow, config_file);

    std::vector<void*> vec_dlsym;
    std::vector<int> myTasks;

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

        vec_dlsym.emplace_back(task_main_);
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
            ((void (*) (int, char**)) (vec_dlsym[myTasks[0]]))(argc, argv);
        };

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
