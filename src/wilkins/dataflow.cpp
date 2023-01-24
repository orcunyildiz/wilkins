#include <wilkins/dataflow.hpp>

string
wilkins::
Dataflow::name()
{
    return name_;
}

string
wilkins::
Dataflow::execGroup()
{
    return execGroup_;
}

string
wilkins::
Dataflow::fullName()
{
    return fullName_;
}

int
wilkins::
Dataflow::in_passthru()
{
    return in_passthru_;
}

int
wilkins::
Dataflow::in_metadata()
{
    return in_metadata_;
}

int
wilkins::
Dataflow::out_passthru()
{
    return out_passthru_;
}

int
wilkins::
Dataflow::out_metadata()
{
    return out_metadata_;
}

int
wilkins::
Dataflow::zerocopy()
{
    return zerocopy_;
}

WilkinsSizes*
wilkins::
Dataflow::sizes()
{
    return &sizes_;
}

// whether this rank is producer, or consumer
bool
wilkins::
Dataflow::is_prod()
{
    return((type_ & WILKINS_PRODUCER_COMM) == WILKINS_PRODUCER_COMM);
}

bool
wilkins::
Dataflow::is_con()
{
    return((type_ & WILKINS_CONSUMER_COMM) == WILKINS_CONSUMER_COMM);
}

bool
wilkins::
Dataflow::is_prod_root()
{
    return world_rank_ == sizes_.prod_start;
}

bool
wilkins::
Dataflow::is_con_root()
{
    return world_rank_ == sizes_.con_start;
}

CommHandle
wilkins::
Dataflow::prod_comm_handle()
{
    return prod_comm_->handle();
}

CommHandle
wilkins::
Dataflow::con_comm_handle()
{
    return con_comm_->handle();
}

wilkins::
Dataflow::Dataflow(CommHandle world_comm,
                   int workflow_size,
                   int workflow_rank,
                   int& io_proc,
                   WilkinsSizes& wilkins_sizes,
                   int prod,
                   int dflow,
                   int con,
                   WorkflowLink wflowLink):
    world_comm_(world_comm),
    world_size_(workflow_size),
    world_rank_(workflow_rank),
    sizes_(wilkins_sizes),
    wflow_prod_id_(prod),
    wflow_dflow_id_(dflow),
    wflow_con_id_(con),
    type_(WILKINS_OTHER_COMM),
    tokens_(0),
    in_passthru_(0),
    in_metadata_(1),
    out_passthru_(0),
    out_metadata_(1),
    zerocopy_(0)
{

    // ensure sizes and starts fit in the world
    if (sizes_.prod_start + sizes_.prod_size > world_size_   ||
            sizes_.con_start + sizes_.con_size > world_size_)
    {
        fprintf(stderr, "Wilkins error: Group sizes of producer, consumer, and dataflow exceed total "
                        "size of world communicator\n");
        return;
    }

    //orc@13-07: lowfive related flags
    in_passthru_ = wflowLink.in_passthru;
    in_metadata_ = wflowLink.in_metadata;
    out_passthru_ = wflowLink.out_passthru;
    out_metadata_ = wflowLink.out_metadata;
    zerocopy_ = wflowLink.zerocopy;

    name_ = wflowLink.name;
    fullName_ = wflowLink.fullName;
    execGroup_ = wflowLink.execGroup;

    // communicator creation -- only applies to MPMD mode for the user codes
    if (!wilkins_master())
    {
        if (world_rank_ >= sizes_.prod_start &&                   // producer
                world_rank_ < sizes_.prod_start + sizes_.prod_size)
        {
            type_ |= WILKINS_PRODUCER_COMM;
            //orc@24-01: supporting subset of writers for prod
            if(sizes_.prod_writers==-1)
                prod_comm_ = new Comm(world_comm, sizes_.prod_start, sizes_.prod_start + sizes_.prod_size - 1);
            else if(world_rank_ < sizes_.prod_start + sizes_.prod_writers)
                prod_comm_ =  new Comm(world_comm, sizes_.prod_start, sizes_.prod_start + sizes_.prod_writers - 1);
            else
                io_proc = 0; //used by wilkins.py to determine which procs should join L5 ops

        }

        if (world_rank_ >= sizes_.con_start &&                    // consumer
                world_rank_ < sizes_.con_start + sizes_.con_size  && sizes_.con_start!=sizes_.prod_start) //orc@08-11: last if condition is to prevent duplicate comms in TP mode
        {
            type_ |= WILKINS_CONSUMER_COMM;
            con_comm_ = new Comm(world_comm, sizes_.con_start, sizes_.con_start + sizes_.con_size - 1);

            tokens_ = wflowLink.tokens;
        }

    }

}

wilkins::
Dataflow::~Dataflow()
{

    if (!wilkins_master())
    {
        if (is_prod())
            delete prod_comm_;
        if (is_con())
            delete con_comm_;
    }

}


