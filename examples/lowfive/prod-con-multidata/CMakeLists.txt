set                         (targets   producer consumer producer_1 consumer_1_2 consumer_2_2 producer_1_2 producer_2_2 consumer_1)

foreach                     (t ${targets})
    add_executable          (${t}_mpmd ${t}.cpp)
    target_link_libraries   (${t}_mpmd ${libraries} wilkins)
    add_library             (${t} SHARED ${t}.cpp)
    target_link_libraries   (${t} ${libraries} wilkins)
    set_target_properties   (${t} PROPERTIES PREFIX "")
    set_target_properties   (${t} PROPERTIES SUFFIX ".so")
endforeach                  ()

add_executable            (wilkins_master wilkins_master.cpp)
target_link_libraries     (wilkins_master ${libraries} wilkins)

install                     (TARGETS ${targets} wilkins_master producer_mpmd consumer_mpmd producer_1_mpmd consumer_1_2_mpmd consumer_2_2_mpmd producer_1_2_mpmd producer_2_2_mpmd consumer_1_mpmd
                            DESTINATION ${CMAKE_INSTALL_PREFIX}/examples/lowfive/prod-con-multidata/
                            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_WRITE GROUP_EXECUTE
                            WORLD_READ WORLD_WRITE WORLD_EXECUTE)

install                     (FILES wilkins_prod_con.yaml wilkins_prod_2cons.yaml wilkins_2prods_con.yaml wilkins_prod_con.sh wilkins_prod_2cons.sh wilkins_2prods_con.sh
                            DESTINATION ${CMAKE_INSTALL_PREFIX}/examples/lowfive/prod-con-multidata/
                            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_WRITE GROUP_EXECUTE
                            WORLD_READ WORLD_WRITE WORLD_EXECUTE)



