# ---------------------------------------------------------------------------
# Python task example (Henson mode).
# Run with:  mpirun -n 2 -l wilkins-master wilkins_prod_con.yaml
# ---------------------------------------------------------------------------

message("Building python example")

install                     (FILES
                            wilkins_prod_con.yaml wilkins_run.sh producer.py consumer.py
                            DESTINATION ${CMAKE_INSTALL_PREFIX}/examples/python/
                            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_WRITE GROUP_EXECUTE
                            WORLD_READ WORLD_WRITE WORLD_EXECUTE)
