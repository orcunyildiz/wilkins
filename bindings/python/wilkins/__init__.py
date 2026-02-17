# ---------------------------------------------------------------------------
# Wilkins - In situ workflow system for heterogeneous task specification
#           and execution for in situ data processing.
#
# Pure-Python implementation (replaces the C++/pybind11 pywilkins module).
# ---------------------------------------------------------------------------

from .types import (
    WILKINS_OTHER_COMM,
    WILKINS_PRODUCER_COMM,
    WILKINS_CONSUMER_COMM,
    WilkinsSizes,
)
from .comm import Comm, comm_rank, comm_size
from .context import (
    wilkins_master,
    wilkins_set_intercomms,
    wilkins_get_intercomms,
    wilkins_set_local_comm,
    wilkins_get_local_comm,
)
from .workflow import (
    LowFivePort,
    WorkflowNode,
    WorkflowLink,
    Workflow,
)
from .dataflow import Dataflow
from .wilkins import (
    LowFiveProperty,
    Wilkins,
    get_local_comm,
    get_intercomms,
)

# Backwards compatibility: the old pybind11 module was imported as
#   from wilkins import pywilkins as w
# To support that pattern, we expose a pywilkins-compatible namespace.
# Users can also import directly from wilkins.wilkins or wilkins.workflow.

__all__ = [
    # types
    "WILKINS_OTHER_COMM",
    "WILKINS_PRODUCER_COMM",
    "WILKINS_CONSUMER_COMM",
    "WilkinsSizes",
    # comm
    "Comm",
    "comm_rank",
    "comm_size",
    # context
    "wilkins_master",
    "wilkins_set_intercomms",
    "wilkins_get_intercomms",
    "wilkins_set_local_comm",
    "wilkins_get_local_comm",
    # workflow
    "LowFivePort",
    "WorkflowNode",
    "WorkflowLink",
    "Workflow",
    # dataflow
    "Dataflow",
    # wilkins
    "LowFiveProperty",
    "Wilkins",
    "get_local_comm",
    "get_intercomms",
]
