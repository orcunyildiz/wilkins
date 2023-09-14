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

void node1_f (int particle,
                int sleep_duration, 
                std::string prefix,
                int input,
                int output,
                 int threads, int mem_blocks,
                 int con_nblocks, int iters)
{

    fmt::print("Entered Middle{}\n", input);

    communicator local = MPI_COMM_WORLD;
    diy::mpi::communicator local_(local);
    std::string inputfile = "outfile_" + std::to_string(input) + ".h5";
    std::string outputfile = "outfile_" + std::to_string(output) + ".h5";

    // --- node1 ranks running user task code ---
    for (size_t i=0; i < iters; i++)
    {
        auto start = std::chrono::steady_clock::now();
        hid_t file        = H5Fopen(inputfile.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
        hid_t dset_particles   = H5Dopen(file, "/group1/particles", H5P_DEFAULT);
        hid_t dspace_particles = H5Dget_space(dset_particles);



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
        // fmt::print(stderr, "Middle{} Read domain: {} {}\n", input, domain.min, domain.max);

        // get global number of particles
        size_t global_num_points;
        {
            std::vector<hsize_t> min_(1), max_(1);
            H5Sget_select_bounds(dspace_particles, min_.data(), max_.data());
            global_num_points = max_[0] + 1;
        }
        fmt::print(stderr, "Middle{} Input Global num points from {}: {}\n", input, inputfile.c_str(), global_num_points);

        // diy setup for the middle task on the consumer side
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

        // fmt::print("Read Middle{} globalnumber{} conblocks {}\n", input,global_num_points,con_nblocks);

        AddBlock                        con_create(con_master, local_num_points, global_num_points, con_nblocks);
        diy::ContiguousAssigner         con_assigner(local_.size(), con_nblocks);
        diy::RegularDecomposer<Bounds>  con_decomposer(dim, domain, con_nblocks);
        con_decomposer.decompose(local_.rank(), con_assigner, con_create);


        // Verify the number of blocks in the con_master object
        // fmt::print("Number of blocks in con_master: {}\n", con_master.size());
        // fmt::print("local_num_points: {}\n", local_num_points);
        // fmt::print("con_nblocks: {}\n", con_nblocks);




        // read the particle data
        con_master.foreach([&](Block* b, const diy::Master::ProxyWithLink& cp)
            { b->read_block_points(cp, dset_particles, global_num_points, con_nblocks); });

        // clean up
        // H5Sclose(dspace_grid);
        H5Sclose(dspace_particles);
        // H5Dclose(dset_grid);
        H5Dclose(dset_particles);
        H5Fclose(file);


        auto end = std::chrono::steady_clock::now();  // End timer
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        // Print the timing information
        fmt::print("Middle{} Input H5Fcreate to H5Fclose Time: {} ms\n", input, duration);
        















        // diy setup for the producer
        diy::FileStorage                prod_storage(prefix);
        diy::Master                     prod_master(local,
            threads,
            mem_blocks,
            &Block::create,
            &Block::destroy,
            &prod_storage,
            &Block::save,
            &Block::load);
        // global_num_points = particle * ;
        // local_num_points = global_num_points / con_nblocks;
        local_num_points = particle;
        global_num_points = local_num_points * con_nblocks;

        AddBlock                        prod_create(prod_master, local_num_points, global_num_points, con_nblocks);
        diy::ContiguousAssigner         prod_assigner(local_.size(), con_nblocks);
        diy::RegularDecomposer<Bounds>  prod_decomposer(dim, domain, con_nblocks);
        prod_decomposer.decompose(local_.rank(), prod_assigner, prod_create);
        // fmt::print("Write Middle{} globalnumber{} conblocks {}\n", input,global_num_points,con_nblocks);


        auto start1 = std::chrono::steady_clock::now();
        //orc@16-05: adding write to node1 for particles
        hid_t file_w = H5Fcreate(outputfile.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT); //TODO: if needed, we can add singleFile flag as in node0
        hid_t group_w = H5Gcreate(file_w, "/group1", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

        std::vector<hsize_t> domain_cnts(DIM);
        // create the file data space for the particles

        domain_cnts[0]  = global_num_points;
        domain_cnts[1]  = DIM;

        hid_t filespace_w = H5Screate_simple(2, &domain_cnts[0], NULL);

        // create the particle dataset with default properties
        hid_t dset_w = H5Dcreate2(group_w, "particles", H5T_IEEE_F32LE, filespace_w, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

        // write the particle data
        prod_master.foreach([&](Block* b, const diy::Master::ProxyWithLink& cp)
            { b->write_block_points(cp, dset_w, con_nblocks); });

        // clean up
        H5Dclose(dset_w);
        H5Sclose(filespace_w);
        H5Gclose(group_w);
        H5Fclose(file_w);


        auto end1 = std::chrono::steady_clock::now();  // End timer
        auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1).count();

        // Print the timing information
        fmt::print("Middle{} Output H5Fcreate to H5Fclose Time: {} ms\n", input, duration1);
        
        // int sleep_duration = 5;
        fmt::print("Sleep {} seconds for Middle{}\n", sleep_duration, input);
        sleep(sleep_duration);


    }
}

int main(int argc, char* argv[])
{
    auto start = std::chrono::steady_clock::now();

    int   dim = DIM;

    MPI_Init(NULL, NULL);

    diy::mpi::communicator    world;

    int iters         = 2;
    iters             = atoi(argv[1]);
    int sleep_duration = atoi(argv[2]);
    int input = atoi(argv[3]);
    int output = atoi(argv[4]);
    int particle = atoi(argv[5]);

    // fmt::print("input is {}\n", input);
    // fmt::print("output is {}\n", output);

    fmt::print("Halo from middle{} with looping for {} iters\n", input, iters);

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

    int con_nblocks = global_nblocks;

    node1_f(particle, sleep_duration, prefix, input, output, threads, mem_blocks, con_nblocks, iters);

    MPI_Finalize();


    auto end = std::chrono::steady_clock::now();  // End timer
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Print the timing information
    fmt::print("Middle{} Task Time: {} ms\n", input, duration);



}
