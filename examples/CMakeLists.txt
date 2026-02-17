# ===========================================================================
# C++ task libraries for use with the Python Wilkins orchestrator.
#
# These CMake targets build the C++ shared libraries (.hx) that are loaded
# by Henson at runtime.  Orchestration is handled by the Python 'wilkins-master'
# entry point (installed via pip).  See examples/lowfive/README.md for details.
# ===========================================================================

if(${lowfive})
    message                     ("Building lowfive example")
    add_subdirectory            (lowfive)
endif(${lowfive})


add_subdirectory                (python)
