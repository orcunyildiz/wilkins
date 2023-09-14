#include    <thread>
#include    <chrono> 
#include    <diy/master.hpp>
#include    <diy/decomposition.hpp>
#include    <diy/assigner.hpp>
#include    <diy/../../examples/opts.h>

#include    <dlfcn.h>

#include    "prod-con-multidata.hpp"

#include <diy/mpi/communicator.hpp>
using communicator = MPI_Comm;
using diy_comm = diy::mpi::communicator;

#include <string>

void consumer_f (int sleep_duration,
                 std::string prefix,
                 int input,
                 int threads, int mem_blocks,
                 int con_nblocks, int iters)
{

    fmt::print("Entered consumer\n");

    communicator local = MPI_COMM_WORLD;
    diy::mpi::communicator local_(local);
    std::string inputfile = "outfile_" + std::to_string(input) + ".h5";



    // --- consumer ranks running user task code ---
    //orc@31-01: adding looping to test the flow control
    for (size_t i=0; i < iters; i++)
    {
        // input = input + i;
        inputfile = "outfile_" + std::to_string(input) + ".h5";

        auto start = std::chrono::steady_clock::now();
        hid_t file        = H5Fopen(inputfile.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
        fmt::print("Opened File name: {}\n", inputfile.c_str());
        // hid_t dset_grid   = H5Dopen(file, "/group1/grid", H5P_DEFAULT);
        // hid_t dspace_grid = H5Dget_space(dset_grid);
        fmt::print("test 0.0v\n");
        hid_t dset_particles   = H5Dopen(file, "/group1/particles", H5P_DEFAULT);
        hid_t dspace_particles = H5Dget_space(dset_particles);
        fmt::print("test 0.1v\n");

        // // get global domain bounds
        // int dim = H5Sget_simple_extent_ndims(dspace_grid);
        // Bounds domain { dim };
        // {
        //     std::vector<hsize_t> min_(dim), max_(dim);
        //     H5Sget_select_bounds(dspace_grid, min_.data(), max_.data());
        //     for (int i = 0; i < dim; ++i)
        //     {
        //         domain.min[i] = min_[i];
        //         domain.max[i] = max_[i];
        //     }
        // }
        // fmt::print(stderr, "Consumer Read domain: {} {}\n", domain.min, domain.max);

        // get global domain bounds
        int dspace_dim = H5Sget_simple_extent_ndims(dspace_particles);  // 2d [particle id][coordinate id]
        std::vector<hsize_t> min_(dspace_dim), max_(dspace_dim);
        H5Sget_select_bounds(dspace_particles, min_.data(), max_.data());
        // fmt::print(stderr, "Dataspace extent: [{}] [{}]\n", fmt::join(min_, ","), fmt::join(max_, ","));
        int dim = max_[dspace_dim - 1] + 1;                             // 3d (extent of coordinate ids)
        Bounds domain { dim };
        {
            for (int i = 0; i < dim; ++i)
            {
                domain.min[i] = 0;
                domain.max[i] = 10;
            }
        }
        // fmt::print(stderr, "Consumer Read domain: {} {}\n", domain.min, domain.max);

        // get global number of particles
        size_t global_num_points;
        {
            std::vector<hsize_t> min_(1), max_(1);
            H5Sget_select_bounds(dspace_particles, min_.data(), max_.data());
            global_num_points = max_[0] + 1;
        }
        fmt::print(stderr, "Consumer Input Global num points from {}: {}\n", inputfile.c_str(), global_num_points);

        // diy setup for the consumer task on the consumer side
        diy::FileStorage                con_storage(prefix);
        diy::Master                     con_master(local_,
            threads,
            mem_blocks,
            &Block::create,
            &Block::destroy,
            &con_storage,
            &Block::save,
            &Block::load);

        size_t local_num_points = global_num_points / con_nblocks;

        AddBlock                        con_create(con_master, local_num_points, global_num_points, con_nblocks);
        diy::ContiguousAssigner         con_assigner(local_.size(), con_nblocks);
        diy::RegularDecomposer<Bounds>  con_decomposer(dim, domain, con_nblocks);
        con_decomposer.decompose(local_.rank(), con_assigner, con_create);
        
        // // read the grid data
        // con_master.foreach([&](Block* b, const diy::Master::ProxyWithLink& cp)
        //     { b->read_block_grid(cp, dset_grid); });

        // Verify the number of blocks in the con_master object
        // fmt::print("Number of blocks in con_master: {}\n", con_master.size());
        // fmt::print("local_num_points: {}\n", local_num_points);
        // fmt::print("con_nblocks: {}\n", con_nblocks);

        // read the particle data
        // fmt::print("DEBUG: before read function\n");
        con_master.foreach([&](Block* b, const diy::Master::ProxyWithLink& cp)
            { b->read_block_points(cp, dset_particles, global_num_points, con_nblocks); });
        // fmt::print("DEBUG: after read function\n");
        // // clean up
        // H5Dclose(dset_grid);
        // H5Sclose(dspace_grid);
        H5Sclose(dspace_particles);
        H5Dclose(dset_particles);
        H5Fclose(file);

        auto end = std::chrono::steady_clock::now();  // End timer
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        // Print the timing information
        fmt::print("Consumer H5Fcreate to H5Fclose Time: {} ms\n", duration);

        //adding sleep here to emulate (2x) slow consumer
        fmt::print("Sleep {} seconds for consumers\n", sleep_duration);
        sleep(sleep_duration);
    }
}

int main(int argc, char* argv[])
{
    auto start = std::chrono::steady_clock::now();

    int   dim = DIM;

    MPI_Init(NULL, NULL);

    diy::mpi::communicator    world;

    // communicator local;
    // MPI_Comm_dup(world, &local);

    int iters         = 2;
    iters             = atoi(argv[1]);
    int sleep_duration = atoi(argv[2]);
    int input = atoi(argv[3]);
    // int producer_count = atoi(argv[4]);

    fmt::print("Halo from consumer with looping for {} iters\n", iters);

    int                       global_nblocks    = world.size();   // global number of blocks

    int                       mem_blocks        = -1;             // all blocks in memory
    int                       threads           = 1;              // no multithreading
    std::string               prefix            = "./DIY.XXXXXX"; // for saving block files out of core

    // get command line arguments
    using namespace opts;
    Options ops;
    ops
        >> Option('b', "blocks",    global_nblocks, "number of blocks")
        >> Option('t', "thread",    threads,        "number of threads")
        >> Option(     "memblks",   mem_blocks,     "number of blocks to keep in memory")
        >> Option(     "prefix",    prefix,         "prefix for external storage")
        ;

    bool verbose, help;
    ops
        >> Option('v', "verbose",   verbose,        "print the block contents")
        >> Option('h', "help",      help,           "show help")
        ;

    if (!ops.parse(argc,argv) || help)
    {
        if (world.rank() == 0)
        {
            std::cout << "Usage: " << argv[0] << " [OPTIONS]\n";
            std::cout << "Generates a grid and random particles in the domain and redistributes them into correct blocks.\n";
            std::cout << ops;
        }
        return 1;
    }

    // consumer will read different block decomposition than the producer
    // producer also needs to know this number so it can match collective operations
    int con_nblocks = pow(2, dim) * global_nblocks;

    consumer_f(sleep_duration, prefix, input, threads, mem_blocks, con_nblocks, iters);

    MPI_Finalize();

    auto end = std::chrono::steady_clock::now();  // End timer
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Print the timing information
    fmt::print("Consumer Task Time: {} ms\n", duration);



}
