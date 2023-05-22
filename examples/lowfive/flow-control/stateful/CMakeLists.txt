set                         (CMAKE_POSITION_INDEPENDENT_CODE ON)

#Adding for Henson: executables as in Wilkins
add_library                 (prod-henson SHARED prod-henson.cpp)
target_link_libraries       (prod-henson ${libraries})
set_target_properties       (prod-henson PROPERTIES PREFIX "")
set_target_properties       (prod-henson PROPERTIES SUFFIX ".hx")

add_library                 (con-henson SHARED con-henson.cpp)
target_link_libraries       (con-henson ${libraries})
set_target_properties       (con-henson PROPERTIES PREFIX "")
set_target_properties       (con-henson PROPERTIES SUFFIX ".hx")

add_library                 (prod1-henson SHARED producer_1-henson.cpp)
target_link_libraries       (prod1-henson ${libraries})
set_target_properties       (prod1-henson PROPERTIES PREFIX "")
set_target_properties       (prod1-henson PROPERTIES SUFFIX ".hx")

add_library                 (con1-henson SHARED consumer_1_2-henson.cpp)
target_link_libraries       (con1-henson ${libraries})
set_target_properties       (con1-henson PROPERTIES PREFIX "")
set_target_properties       (con1-henson PROPERTIES SUFFIX ".hx")

add_library                 (con2-henson SHARED consumer_2_2-henson.cpp)
target_link_libraries       (con2-henson ${libraries})
set_target_properties       (con2-henson PROPERTIES PREFIX "")
set_target_properties       (con2-henson PROPERTIES SUFFIX ".hx")

if                          (NOT APPLE)
   set                      (linker_flags "-pie -Wl,--export-dynamic")
   set                      (linker_flags "${linker_flags} -Wl,-u,henson_set_contexts,-u,henson_set_namemap")
else                        ()
   set                      (linker_flags "-Wl,-u,_henson_set_contexts,-u,_henson_set_namemap")
endif                       (NOT APPLE)

set_target_properties    (prod-henson         PROPERTIES LINK_FLAGS ${linker_flags})
set_target_properties    (con-henson          PROPERTIES LINK_FLAGS ${linker_flags})
set_target_properties    (prod1-henson        PROPERTIES LINK_FLAGS ${linker_flags})
set_target_properties    (con1-henson         PROPERTIES LINK_FLAGS ${linker_flags})
set_target_properties    (con2-henson         PROPERTIES LINK_FLAGS ${linker_flags})

install                     (TARGETS prod-henson prod1-henson con-henson con1-henson con2-henson
                            DESTINATION ${CMAKE_INSTALL_PREFIX}/examples/lowfive/flow-control/stateful/
                            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_WRITE GROUP_EXECUTE
                            WORLD_READ WORLD_WRITE WORLD_EXECUTE)

install                     (FILES wilkins_prod_con.yaml wilkins_prod_con_singleFile.yaml wilkins_prod_2cons.yaml run_stateful.sh ../../wilkins-master.py
                            DESTINATION ${CMAKE_INSTALL_PREFIX}/examples/lowfive/flow-control/stateful/
                            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_WRITE GROUP_EXECUTE
                            WORLD_READ WORLD_WRITE WORLD_EXECUTE)
