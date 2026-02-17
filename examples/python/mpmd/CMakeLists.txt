# ---------------------------------------------------------------------------
# Python task example (MPMD mode -- tasks launched as separate MPI programs).
# Run with:  mpirun -np 3 -l python ./prod.py : -np 1 python ./con.py
# ---------------------------------------------------------------------------

message("Building python example")

install                     (FILES
                            wilkins_prod_con.yaml wilkins_prod_con.sh prod.py con.py
                            DESTINATION ${CMAKE_INSTALL_PREFIX}/examples/python/mpmd/
                            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_WRITE GROUP_EXECUTE
                            WORLD_READ WORLD_WRITE WORLD_EXECUTE)
