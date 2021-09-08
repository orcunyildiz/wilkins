set                         (targets   producer consumer producer_1 consumer_1_2 consumer_2_2 producer_1_2 producer_2_2 consumer_1)

foreach                     (t ${targets})
  add_executable            (${t} ${t}.cpp)
  target_link_libraries     (${t} ${libraries} wilkins)
endforeach                  ()

install                     (TARGETS ${targets}
                            DESTINATION ${CMAKE_INSTALL_PREFIX}/examples/lowfive/prod-con-multidata/
                            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_WRITE GROUP_EXECUTE
                            WORLD_READ WORLD_WRITE WORLD_EXECUTE)

install                     (FILES wilkins_prod_con.yaml wilkins_prod_2cons.yaml wilkins_2prods_con.yaml wilkins_prod_con.sh wilkins_prod_2cons.sh wilkins_2prods_con.sh
                            DESTINATION ${CMAKE_INSTALL_PREFIX}/examples/lowfive/prod-con-multidata/
                            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_WRITE GROUP_EXECUTE
                            WORLD_READ WORLD_WRITE WORLD_EXECUTE)



