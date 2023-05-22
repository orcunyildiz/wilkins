#include    <thread>

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

void node1_f (std::string prefix,
                 int threads, int mem_blocks,
                 int con_nblocks, int iters)
{

    fmt::print("Entered node1\n");

    communicator local = MPI_COMM_WORLD;
    diy::mpi::communicator local_(local);

    // --- node1 ranks running user task code ---
    for (size_t i=0; i < iters; i++)
    {

        hid_t file        = H5Fopen("outfile0.h5", H5F_ACC_RDONLY, H5P_DEFAULT);
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

        // diy setup for the node1 task on the consumer side
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

        //orc@16-05: adding write to node1 for particles
        hid_t file_w = H5Fcreate("outfile1.h5", H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT); //TODO: if needed, we can add singleFile flag as in node0
        hid_t group_w = H5Gcreate(file_w, "/group1", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

        std::vector<hsize_t> domain_cnts(DIM);
        // create the file data space for the particles
        domain_cnts[0]  = global_num_points;
        domain_cnts[1]  = DIM;

        hid_t filespace_w = H5Screate_simple(2, &domain_cnts[0], NULL);

        // create the particle dataset with default properties
        hid_t dset_w = H5Dcreate2(group_w, "particles", H5T_IEEE_F32LE, filespace_w, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

        // write the particle data
        con_master.foreach([&](Block* b, const diy::Master::ProxyWithLink& cp)
            { b->write_block_points(cp, dset_w, con_nblocks); });

        // clean up
        H5Dclose(dset_w);
        H5Sclose(filespace_w);
        H5Gclose(group_w);
        H5Fclose(file_w);

    }
}

int main(int argc, char* argv[])
{

    int   dim = DIM;

    MPI_Init(NULL, NULL);

    diy::mpi::communicator    world;

    int iters         = 2;
    iters             = atoi(argv[1]);

    fmt::print("Halo from node1 with looping for {} iters\n", iters);

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

    // node1 will read different block decomposition than the producer
    // producer also needs to know this number so it can match collective operations
    int con_nblocks = pow(2, dim) * global_nblocks;

    node1_f(prefix, threads, mem_blocks, con_nblocks, iters);

    MPI_Finalize();
}
