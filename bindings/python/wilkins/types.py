# ---------------------------------------------------------------------------
#
# wilkins typedefs, structs -- Python port of include/wilkins/types.hpp
#
# --------------------------------------------------------------------------

from dataclasses import dataclass, field

# communicator types (bitmask values)
WILKINS_OTHER_COMM: int = 0x00
WILKINS_PRODUCER_COMM: int = 0x01
WILKINS_CONSUMER_COMM: int = 0x04


@dataclass
class WilkinsSizes:
    """Sizes and starting ranks for producer/consumer communicators."""
    prod_size: int = 0       # size (number of processes) of producer communicator
    prod_writers: int = -1   # (optional) size (number of processes) of writers in producer
    con_size: int = 0        # size (number of processes) of consumer communicator
    prod_start: int = 0      # starting world process rank of producer communicator
    con_start: int = 0       # starting world process rank of consumer communicator
