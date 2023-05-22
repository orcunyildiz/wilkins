set                         (CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library                 (node0 SHARED node0.cpp)
target_link_libraries       (node0 ${libraries})
set_target_properties       (node0 PROPERTIES PREFIX "")
set_target_properties       (node0 PROPERTIES SUFFIX ".hx")

add_library                 (node1 SHARED node1.cpp)
target_link_libraries       (node1 ${libraries})
set_target_properties       (node1 PROPERTIES PREFIX "")
set_target_properties       (node1 PROPERTIES SUFFIX ".hx")

add_library                 (node2 SHARED node2.cpp)
target_link_libraries       (node2 ${libraries})
set_target_properties       (node2 PROPERTIES PREFIX "")
set_target_properties       (node2 PROPERTIES SUFFIX ".hx")

if                          (NOT APPLE)
   set                      (linker_flags "-pie -Wl,--export-dynamic")
   set                      (linker_flags "${linker_flags} -Wl,-u,henson_set_contexts,-u,henson_set_namemap")
else                        ()
   set                      (linker_flags "-Wl,-u,_henson_set_contexts,-u,_henson_set_namemap")
endif                       (NOT APPLE)

set_target_properties    (node0        PROPERTIES LINK_FLAGS ${linker_flags})
set_target_properties    (node1        PROPERTIES LINK_FLAGS ${linker_flags})
set_target_properties    (node2        PROPERTIES LINK_FLAGS ${linker_flags})

install                     (TARGETS node0 node1 node2
                            DESTINATION ${CMAKE_INSTALL_PREFIX}/examples/lowfive/cycle/
                            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_WRITE GROUP_EXECUTE
                            WORLD_READ WORLD_WRITE WORLD_EXECUTE)

install                     (FILES wilkins_cycle.yaml run_cycle.sh ../wilkins-master.py
                            DESTINATION ${CMAKE_INSTALL_PREFIX}/examples/lowfive/cycle/
                            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_WRITE GROUP_EXECUTE
                            WORLD_READ WORLD_WRITE WORLD_EXECUTE)
