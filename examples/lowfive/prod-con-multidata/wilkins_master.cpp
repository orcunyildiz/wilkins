#include    <thread>
#include <iostream>
#include <system_error>

#include    <dlfcn.h>

#include    <diy/master.hpp>
using communicator = MPI_Comm;

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

    std::vector<std::thread> myThreads;
    std::thread task_thread;

    communicator local = wilkins->local_comm_handle();
    communicator local_dup;
    MPI_Comm_dup(local, &local_dup); //duplicate of the local communicator for SP mode

    std::vector<communicator> intercomms = wilkins->build_intercomms();

    //comm vectors for the shared mode
    std::vector<communicator*> local_comms;
    std::vector<std::vector<communicator>> vec_intercomms_shared;

    communicator world_dup;
    MPI_Comm_dup(world, &world_dup);

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
        //shared (TP) mode
        fmt::print("shared mode with {} tasks sharing the same resources\n", myTasks.size());

        //creating local and intercomms
        for (size_t i = 0; i < workflow.nodes.size(); i++)
        {

     	    //orc@05-11: creating duplicate local comms for each task
            communicator* local_dup = new communicator();
            //local_dup->duplicate(local); //orc@24-10: required for supporting free mixing of TP & SP
            MPI_Comm_dup(local, local_dup); //orc@24-10: required for supporting free mixing of TP & SP
            local_comms.emplace_back(local_dup);

            //orc@05-11: splitting the main intercomm vector per task in the shared mode
            std::vector<int> intercomms_shared = wilkins->build_intercomms(workflow.nodes[i].func);
            std::vector<communicator> intercomms_tp;

            for (size_t i = 0; i < intercomms_shared.size(); i++)
            {
             	if (intercomms_shared[i] ==1)
                    intercomms_tp.emplace_back(intercomms[i]);

            }

            vec_intercomms_shared.emplace_back(intercomms_tp);

        }

        for (size_t i = 0; i < myTasks.size(); i++)
        {

            //set intercomms
            auto task_intercomm = [&, i]()
            {
                ((void (*) (void*)) (vec_dlsym[(3*myTasks[i])+1]))(&vec_intercomms_shared[i]);
            };

            task_intercomm();

            //set local comms
            auto task_local_comm = [&, i]()
            {
             	((void (*) (void*)) (vec_dlsym[(3*myTasks[i])+2]))(&(*local_comms[i]));
            };

            task_local_comm();

            //creating the user puppets as threads
            auto task_main = [&, i]()
            {
                ((void (*) (int, char**)) (vec_dlsym[3*myTasks[i]]))(argc, argv);
            };

            task_thread = std::thread(task_main);

            myThreads.push_back(std::move(task_thread));

        }

        //running the user puppets
        for(auto& th : myThreads)
        {
            try
            {
                th.join();
            }
            catch(const std::system_error& e)
            {
                std::cout << "Caught system_error with code " << e.code()
                      << " meaning " << e.what() << '\n';
            }

        }

    }
    else if (myTasks.size() == 1)
    {
        //SP mode
        auto task_intercomm = [&]()
        {
            ((void (*) (void*)) (vec_dlsym[(3*myTasks[0])+1]))(&intercomms);
        };

        auto task_local_comm = [&]()
        {
            ((void (*) (void*)) (vec_dlsym[(3*myTasks[0])+2]))(&local_dup);
        };

        auto task_main = [&]()
        {
            ((void (*) (int, char**)) (vec_dlsym[(3 * myTasks[0])]))(argc, argv);
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

    //cleanup for shared mode
    for (size_t i = 0; i < local_comms.size(); i++)
         delete local_comms[i];

}
