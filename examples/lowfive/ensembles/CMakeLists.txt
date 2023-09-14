set                         (CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library                 (prod SHARED producer.cpp)
target_link_libraries       (prod ${libraries})
set_target_properties       (prod PROPERTIES PREFIX "")
set_target_properties       (prod PROPERTIES SUFFIX ".hx")

add_library                 (con SHARED consumer.cpp)
target_link_libraries       (con ${libraries})
set_target_properties       (con PROPERTIES PREFIX "")
set_target_properties       (con PROPERTIES SUFFIX ".hx")

add_library                 (con_sf SHARED consumer_stateful.cpp)
target_link_libraries       (con_sf ${libraries})
set_target_properties       (con_sf PROPERTIES PREFIX "")
set_target_properties       (con_sf PROPERTIES SUFFIX ".hx")

if                          (NOT APPLE)
   set                      (linker_flags "-pie -Wl,--export-dynamic")
   set                      (linker_flags "${linker_flags} -Wl,-u,henson_set_contexts,-u,henson_set_namemap")
else                        ()
   set                      (linker_flags "${linker_flags} -Wl,-u,_henson_set_contexts,-u,_henson_set_namemap")
endif                       (NOT APPLE)

set_target_properties    (prod         PROPERTIES LINK_FLAGS ${linker_flags})
set_target_properties    (con          PROPERTIES LINK_FLAGS ${linker_flags})
set_target_properties    (con_sf          PROPERTIES LINK_FLAGS ${linker_flags})

install                     (TARGETS prod con con_sf
                            DESTINATION ${CMAKE_INSTALL_PREFIX}/examples/lowfive/ensembles/
                            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_WRITE GROUP_EXECUTE
                            WORLD_READ WORLD_WRITE WORLD_EXECUTE)

install                     (FILES wilkins_prod_con.yaml generateRunScript.sh ../wilkins-master.py
                            DESTINATION ${CMAKE_INSTALL_PREFIX}/examples/lowfive/ensembles/
                            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_WRITE GROUP_EXECUTE
                            WORLD_READ WORLD_WRITE WORLD_EXECUTE)
