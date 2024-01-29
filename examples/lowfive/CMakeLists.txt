#add_subdirectory            (prod-con-multidata)
add_subdirectory             (flow-control)
add_subdirectory             (cycle)
add_subdirectory             (ensembles)

install                     (FILES wilkins-master.py
                            DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
                            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_WRITE GROUP_EXECUTE
                            WORLD_READ WORLD_WRITE WORLD_EXECUTE)
