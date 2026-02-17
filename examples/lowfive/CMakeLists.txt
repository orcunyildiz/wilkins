# ===========================================================================
# C++ task libraries for LowFive examples.
#
# These targets build .hx shared libraries loaded by Henson at runtime.
# Orchestration is now done by the Python 'wilkins-master' entry point:
#
#     pip install .                       # from the repository root
#     wilkins-master <config>.yaml        # run a workflow
#
# The old wilkins-master.py script is kept for reference but is deprecated.
# See examples/lowfive/README.md for full instructions.
# ===========================================================================

#add_subdirectory            (prod-con-multidata)       # DEPRECATED: uses C++ Wilkins API (MPMD mode)
add_subdirectory             (flow-control)
add_subdirectory             (cycle)
add_subdirectory             (ensembles)

install                     (FILES generate_run_script.py
                            DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
                            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_WRITE GROUP_EXECUTE
                            WORLD_READ WORLD_WRITE WORLD_EXECUTE)
