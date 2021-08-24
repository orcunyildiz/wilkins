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
                 Bounds domain,
                 int con_nblocks, int dim, size_t global_num_points)
{
    fmt::print("Entered consumer\n");

    l5::DistMetadataVOL vol_plugin = wilkins->build_lowfive();
    hid_t plist = wilkins->plist();

    communicator local = wilkins->local_comm_handle();

    // --- consumer ranks running user task code ---


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

    // open the file and the dataset
    hid_t file = H5Fopen("outfile.h5", H5F_ACC_RDONLY, plist);
    hid_t dset = H5Dopen(file, "/group1/grid", H5P_DEFAULT);

    // read the grid data
    con_master.foreach([&](Block* b, const diy::Master::ProxyWithLink& cp)
            { b->read_block_grid(cp, dset); });

    // clean up
    H5Dclose(dset);

    // open the particle dataset
    dset = H5Dopen(file, "/group1/particles", H5P_DEFAULT);

    // read the particle data
    con_master.foreach([&](Block* b, const diy::Master::ProxyWithLink& cp)
            { b->read_block_points(cp, dset, global_num_points, con_nblocks); });

    // clean up
    H5Dclose(dset);
    H5Fclose(file);
    H5Pclose(plist);
}

int main(int argc, char* argv[])
{

    int   dim = DIM;

    diy::mpi::environment     env(argc, argv, MPI_THREAD_MULTIPLE);
    diy::mpi::communicator    world;

    //orc@02-07: wilkins init
    Workflow workflow;
    Workflow::make_wflow_from_json(workflow, "wilkins_2nodes.json");

    // create wilkins
    Wilkins* wilkins = new Wilkins(MPI_COMM_WORLD, workflow);

    fmt::print("Halo from Wilkins\n");

    int                       global_nblocks    = world.size();   // global number of blocks
    int                       mem_blocks        = -1;             // all blocks in memory
    int                       threads           = 1;              // no multithreading
    std::string               prefix            = "./DIY.XXXXXX"; // for saving block files out of core
    // opts does not handle bool correctly, using int instead
    int                       metadata          = 1;              // build in-memory metadata
    int                       passthru          = 0;              // write file to disk
    size_t                    local_npoints     = 100;            // points per block

    // default global data bounds
    Bounds domain { dim };
    for (auto i = 0; i < dim; i++)
    {
        domain.min[i] = 0;
        domain.max[i] = 10;
    }

    // get command line arguments
    using namespace opts;
    Options ops;
    ops
        >> Option('n', "number",    local_npoints,  "number of points per block")
        >> Option('b', "blocks",    global_nblocks, "number of blocks")
        >> Option('t', "thread",    threads,        "number of threads")
        >> Option(     "memblks",   mem_blocks,     "number of blocks to keep in memory")
        >> Option(     "prefix",    prefix,         "prefix for external storage")
        >> Option('m', "memory",    metadata,       "build and use in-memory metadata")
        >> Option('f', "file",      passthru,       "write file to disk")
        ;
    ops
        >> Option('x',  "max-x",    domain.max[0],  "domain max x")
        >> Option('y',  "max-y",    domain.max[1],  "domain max y")
        >> Option('z',  "max-z",    domain.max[2],  "domain max z")
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

    if (!(metadata + passthru))
    {
        fmt::print(stderr, "Error: Either metadata or passthru must be enabled. Both cannot be disabled.\n");
        abort();
    }

    // ---  all ranks running workflow runtime system code ---

    size_t global_npoints = global_nblocks * local_npoints;         // all block have same number of points

    // consumer will read different block decomposition than the producer
    // producer also needs to know this number so it can match collective operations
    int con_nblocks = pow(2, dim) * global_nblocks;

    consumer_f(wilkins, prefix, threads, mem_blocks, domain, con_nblocks, dim, global_npoints);
}
