# ---------------------------------------------------------------------------
#
# wilkins global context -- Python port of include/wilkins/context.h
#                           and src/wilkins/context.cpp
#
# Provides global state management for the Wilkins master mode.
# In the C++ version, these are used with dlsym for shared library injection.
# In the pure-Python version, these serve as module-level state for the
# master driver to inject communicators into task code.
#
# --------------------------------------------------------------------------

_local_comm = None
_intercomms = None


def wilkins_master():
    """Return True if running under the wilkins master driver.

    Returns True when intercomms have been set (i.e., master mode),
    False in MPMD mode.
    """
    return _intercomms is not None


def wilkins_set_intercomms(intercomms):
    """Set the global intercommunicators (called by master to inject into tasks)."""
    global _intercomms
    _intercomms = intercomms


def wilkins_get_intercomms():
    """Return the stored intercommunicators."""
    return list(_intercomms)


def wilkins_set_local_comm(local):
    """Set the global local communicator."""
    global _local_comm
    _local_comm = local


def wilkins_get_local_comm():
    """Return the stored local communicator."""
    return _local_comm
