set                         (targets   producer consumer producer_1 consumer_1 consumer_2)

foreach                     (t ${targets})
  add_executable            (${t} ${t}.cpp)
  target_link_libraries     (${t} ${libraries} wilkins)
endforeach                  ()

install                     (TARGETS ${targets}
                            DESTINATION ${CMAKE_INSTALL_PREFIX}/examples/lowfive/prod-con-multidata/
                            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_WRITE GROUP_EXECUTE
                            WORLD_READ WORLD_WRITE WORLD_EXECUTE)

install                     (FILES wilkins_2nodes.yaml wilkins_3nodes.yaml wilkins_2nodes.json wilkins_3nodes.json wilkins_2nodes.sh wilkins_3nodes.sh
                            DESTINATION ${CMAKE_INSTALL_PREFIX}/examples/lowfive/prod-con-multidata/
                            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_WRITE GROUP_EXECUTE
                            WORLD_READ WORLD_WRITE WORLD_EXECUTE)



