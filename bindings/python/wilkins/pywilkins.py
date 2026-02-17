# ---------------------------------------------------------------------------
# Backwards-compatibility shim for the old pybind11 ``pywilkins`` module.
#
# The original C++ extension module was imported as:
#     from wilkins import pywilkins as w
#     w.Workflow(), w.Wilkins(...), w.get_local_comm(...), etc.
#
# This module re-exports the same names from the pure-Python implementation
# so that existing driver code continues to work without changes.
# ---------------------------------------------------------------------------

from .workflow import Workflow, WorkflowNode, WorkflowLink
from .wilkins import Wilkins, LowFiveProperty, get_local_comm, get_intercomms

__all__ = [
    "Workflow",
    "WorkflowNode",
    "WorkflowLink",
    "Wilkins",
    "LowFiveProperty",
    "get_local_comm",
    "get_intercomms",
]
