#include    <thread>

#include    <diy/master.hpp>
#include    <diy/decomposition.hpp>
#include    <diy/assigner.hpp>
#include    <diy/../../examples/opts.h>

#include    <dlfcn.h>

#include    "prod-con-multidata.hpp"

#include <diy/mpi/communicator.hpp>
using communicator = diy::mpi::communicator;

//orc@02-07: adding wilkins headers
#include <wilkins/wilkins.hpp>

using namespace wilkins;

// --- ranks of consumer task 2 in multiple prod-cons example---
void consumer2_f (Wilkins* wilkins,
                 std::string prefix,
                 int threads, int mem_blocks,
                 int con_nblocks)
{
    fmt::print("Entered consumer2\n");

    l5::DistMetadataVOL& vol_plugin = wilkins->init();
    hid_t plist = wilkins->plist();

    communicator local = wilkins->local_comm_handle();

    // --- consumer ranks running user task code ---
    // open the file, the dataset and the dataspace
    hid_t file   = H5Fopen("outfile.h5", H5F_ACC_RDONLY, plist);
    hid_t dset   = H5Dopen(file, "/group1/particles", H5P_DEFAULT);
    hid_t dspace = H5Dget_space(dset);

  // get global domain bounds
    int dspace_dim = H5Sget_simple_extent_ndims(dspace);  // 2d [particle id][coordinate id]
    std::vector<hsize_t> min_(dspace_dim), max_(dspace_dim);
    H5Sget_select_bounds(dspace, min_.data(), max_.data());
    fmt::print(stderr, "Dataspace extent: [{}] [{}]\n", fmt::join(min_, ","), fmt::join(max_, ","));
    int dim = max_[dspace_dim - 1] + 1;                             // 3d (extent of coordinate ids)
    Bounds domain { dim };
    {
        // because these are particles in 3d, any 3d decomposition will do
        // as long as it can be decomposed discretely into con_blocks
        // using con_blocks^dim, larger than necessary
        // no attempt to have points be inside of the block bounds (maybe not realistic)
        for (int i = 0; i < dim; ++i)
        {
            domain.min[i] = 0;
            domain.max[i] = con_nblocks;
        }
    }
    fmt::print(stderr, "Read domain: {} {}\n", domain.min, domain.max);


    // get global number of particles
    size_t global_num_points;
    {
        std::vector<hsize_t> min_(1), max_(1);
        H5Sget_select_bounds(dspace, min_.data(), max_.data());
        global_num_points = max_[0] + 1;
    }
    fmt::print(stderr, "Global num points: {}\n", global_num_points);

    // diy setup for the consumer task on the consumer side
    diy::FileStorage                con_storage(prefix);
    diy::Master                     con_master(local,
            threads,
            mem_blocks,
            &Block::create,
            &Block::destroy,
            &con_storage,
            &Block::save,
            &Block::load);
    size_t local_num_points = global_num_points / con_nblocks;
    AddBlock                        con_create(con_master, local_num_points, global_num_points, con_nblocks);
    diy::ContiguousAssigner         con_assigner(local.size(), con_nblocks);
    diy::RegularDecomposer<Bounds>  con_decomposer(dim, domain, con_nblocks);
    con_decomposer.decompose(local.rank(), con_assigner, con_create);

    // read the particle data
    con_master.foreach([&](Block* b, const diy::Master::ProxyWithLink& cp)
            { b->read_block_points(cp, dset, global_num_points, con_nblocks); });

    // clean up
    H5Sclose(dspace);
    H5Dclose(dset);
    H5Fclose(file);
    H5Pclose(plist);
}

int main(int argc, char* argv[])
{

    int   dim = DIM;

    //orc@26-10: Running under MPMD mode, no wilkins_master
    if(!wilkins_master())
        MPI_Init(NULL, NULL);
        //diy::mpi::environment     env(argc, argv, MPI_THREAD_MULTIPLE);

    diy::mpi::communicator    world;

    // create wilkins
    std::string config_file = argv[1];
    Wilkins* wilkins = new Wilkins(MPI_COMM_WORLD, config_file);

    fmt::print("Halo from Wilkins with configuration for {}\n", config_file.c_str());

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

    consumer2_f(wilkins, prefix, threads, mem_blocks, con_nblocks);

    if(!wilkins_master())
        MPI_Finalize();
}
