set                         (CMAKE_POSITION_INDEPENDENT_CODE ON)

#Adding for Henson: executables as in Wilkins
add_library                 (sim SHARED simulation.cpp)
target_link_libraries       (sim ${libraries})
set_target_properties       (sim PROPERTIES PREFIX "")
set_target_properties       (sim PROPERTIES SUFFIX ".hx")

add_library                 (ana SHARED analysis.cpp)
target_link_libraries       (ana ${libraries})
set_target_properties       (ana PROPERTIES PREFIX "")
set_target_properties       (ana PROPERTIES SUFFIX ".hx")

if                          (NOT APPLE)
   set                      (linker_flags "-pie -Wl,--export-dynamic")
   set                      (linker_flags "${linker_flags} -Wl,-u,henson_set_contexts,-u,henson_set_namemap")
else                        ()
   set                      (linker_flags "${linker_flags} -Wl,-u,_henson_set_contexts,-u,_henson_set_namemap")
endif                       (NOT APPLE)

set_target_properties    (sim         PROPERTIES LINK_FLAGS ${linker_flags})
set_target_properties    (ana          PROPERTIES LINK_FLAGS ${linker_flags})

install                     (TARGETS sim ana
                            DESTINATION ${CMAKE_INSTALL_PREFIX}/examples/lowfive/flow-control/stateless/
                            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_WRITE GROUP_EXECUTE
                            WORLD_READ WORLD_WRITE WORLD_EXECUTE)

install                     (FILES wilkins_prod_con.yaml wilkins_prod_con_singleFile.yaml run_stateless.sh ../../wilkins-master.py
                            DESTINATION ${CMAKE_INSTALL_PREFIX}/examples/lowfive/flow-control/stateless/
                            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_WRITE GROUP_EXECUTE
                            WORLD_READ WORLD_WRITE WORLD_EXECUTE)
