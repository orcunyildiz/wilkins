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

// --- ranks of consumer task ---
void consumer_f (Wilkins* wilkins,
                 std::string prefix,
                 int threads, int mem_blocks,
                 int con_nblocks)
{
    fmt::print("Entered consumer\n");

    l5::DistMetadataVOL vol_plugin = wilkins->build_lowfive();
    hid_t plist = wilkins->plist();

    communicator local = wilkins->local_comm_handle();

    //orc@08-09: adapting to the current lowfive API TODO: we can call this inside build_lowfive() 
    vol_plugin.data_intercomm("outfile.h5", "/group1/grid", 0);
    vol_plugin.data_intercomm("outfile.h5", "/group1/particles", 0);

    // --- consumer ranks running user task code ---


    // open the file and the dataset
    hid_t file        = H5Fopen("outfile.h5", H5F_ACC_RDONLY, plist);
    hid_t dset_grid   = H5Dopen(file, "/group1/grid", H5P_DEFAULT);
    hid_t dspace_grid = H5Dget_space(dset_grid);

    hid_t dset_particles   = H5Dopen(file, "/group1/particles", H5P_DEFAULT);
    hid_t dspace_particles = H5Dget_space(dset_particles);

    // get global domain bounds
    int dim = H5Sget_simple_extent_ndims(dspace_grid);
    Bounds domain { dim };
    {
        std::vector<hsize_t> min_(dim), max_(dim);
        H5Sget_select_bounds(dspace_grid, min_.data(), max_.data());
        for (int i = 0; i < dim; ++i)
        {
            domain.min[i] = min_[i];
            domain.max[i] = max_[i];
        }
    }
    fmt::print(stderr, "Read domain: {} {}\n", domain.min, domain.max);

    // get global number of particles
    size_t global_num_points;
    {
        std::vector<hsize_t> min_(1), max_(1);
        H5Sget_select_bounds(dspace_particles, min_.data(), max_.data());
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

    // read the grid data
    con_master.foreach([&](Block* b, const diy::Master::ProxyWithLink& cp)
            { b->read_block_grid(cp, dset_grid); });

    // read the particle data
    con_master.foreach([&](Block* b, const diy::Master::ProxyWithLink& cp)
            { b->read_block_points(cp, dset_particles, global_num_points, con_nblocks); });

    // clean up
    H5Sclose(dspace_grid);
    H5Sclose(dspace_particles);
    H5Dclose(dset_grid);
    H5Dclose(dset_particles);
    H5Fclose(file);
    H5Pclose(plist);
}

int main(int argc, char* argv[])
{

    int   dim = DIM;

    diy::mpi::environment     env(argc, argv, MPI_THREAD_MULTIPLE);
    diy::mpi::communicator    world;

    // create wilkins
    Wilkins* wilkins = new Wilkins(MPI_COMM_WORLD, "wilkins_2nodes.yaml");
    //Wilkins* wilkins = new Wilkins(MPI_COMM_WORLD, "wilkins_2nodes.json");

    fmt::print("Halo from Wilkins\n");

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

    consumer_f(wilkins, prefix, threads, mem_blocks, con_nblocks);
}
