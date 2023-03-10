#include    <thread>

#include    <diy/master.hpp>
#include    <diy/decomposition.hpp>
#include    <diy/assigner.hpp>
#include    <diy/../../examples/opts.h>

#include    <dlfcn.h>

#include    "prod-con-multidata.hpp"

#include <diy/mpi/communicator.hpp>
//using communicator = diy::mpi::communicator;
using communicator = MPI_Comm;
using diy_comm = diy::mpi::communicator;

void consumer1_f (std::string prefix,
                 int threads, int mem_blocks,
                 int con_nblocks, int iters, int latest)
{
    fmt::print("Entered consumer1\n");

    communicator local = MPI_COMM_WORLD; //orc@05-12: henson-mpi replaces the MPI_COMM_WORLD with the local comm, hence, no code changes are necessary.
    diy::mpi::communicator local_(local);

    // --- consumer ranks running user task code ---
    //orc@31-01: adding looping to test the flow control
    for (size_t i=0; i < iters; i++)
    {
        //emulating fast producer-slow consumer coupling with sleep
        if (latest)
            sleep(10);

	// open the file, the dataset, and the dataspace
	//hid_t file   = H5Fopen("outfile.h5", H5F_ACC_RDONLY, plist);
	hid_t file   = H5Fopen("outfile.h5", H5F_ACC_RDONLY, H5P_DEFAULT);
	hid_t dset   = H5Dopen(file, "/group1/grid", H5P_DEFAULT);
	hid_t dspace = H5Dget_space(dset);

	// get global domain bounds
	int dim = H5Sget_simple_extent_ndims(dspace);
	Bounds domain { dim };
	{
	    std::vector<hsize_t> min_(dim), max_(dim);
	    H5Sget_select_bounds(dspace, min_.data(), max_.data());
	    for (int i = 0; i < dim; ++i)
	    {
                domain.min[i] = min_[i];
		domain.max[i] = max_[i];
	    }
	}
	fmt::print(stderr, "Read domain: {} {}\n", domain.min, domain.max);

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
	AddBlock                        con_create(con_master, 0, 0, con_nblocks);
	diy::ContiguousAssigner         con_assigner(local_.size(), con_nblocks);
	diy::RegularDecomposer<Bounds>  con_decomposer(dim, domain, con_nblocks);
	con_decomposer.decompose(local_.rank(), con_assigner, con_create);

	// read the grid data
	con_master.foreach([&](Block* b, const diy::Master::ProxyWithLink& cp)
	    { b->read_block_grid(cp, dset); });

	// clean up
	H5Sclose(dspace);
	H5Dclose(dset);
	H5Fclose(file);
    }
}

int main(int argc, char* argv[])
{

    int   dim = DIM;

    MPI_Init(NULL, NULL);
    diy::mpi::communicator    world;

    //TODO: Will move the looping logic to wilkins.py, then consumer doesn't need to know the number of iters/timesteps
    int iters         = 1;
    iters             = atoi(argv[1]);

    int latest         = 0;
    latest             = atoi(argv[2]);

    fmt::print("Halo from Wilkins con1 with configuration for {} iters w latest/sleep set to {}\n", iters, latest);

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

    consumer1_f(prefix, threads, mem_blocks, con_nblocks, iters, latest);

    MPI_Finalize();
}
