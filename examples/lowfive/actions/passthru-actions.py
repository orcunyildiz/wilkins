from wilkins.utils import setup_passthru_callbacks

def prod_callback(vol, rank):
    setup_passthru_callbacks(vol, "producer")

def con_callback(vol, rank, pl_con):
    setup_passthru_callbacks(vol, "consumer", pl_con)

