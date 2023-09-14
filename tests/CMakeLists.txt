add_library                 (node0-test SHARED ../examples/lowfive/cycle/node0.cpp)
target_link_libraries       (node0-test ${libraries})
set_target_properties       (node0-test PROPERTIES PREFIX "")
set_target_properties       (node0-test PROPERTIES SUFFIX ".hx")

add_library                 (node1-test SHARED ../examples/lowfive/cycle/node1.cpp)
target_link_libraries       (node1-test ${libraries})
set_target_properties       (node1-test PROPERTIES PREFIX "")
set_target_properties       (node1-test PROPERTIES SUFFIX ".hx")

add_library                 (node2-test SHARED ../examples/lowfive/cycle/node2.cpp)
target_link_libraries       (node2-test ${libraries})
set_target_properties       (node2-test PROPERTIES PREFIX "")
set_target_properties       (node2-test PROPERTIES SUFFIX ".hx")

add_library                 (prod-test SHARED ../examples/lowfive/flow-control/stateful/prod-henson.cpp)
target_link_libraries       (prod-test ${libraries})
set_target_properties       (prod-test PROPERTIES PREFIX "")
set_target_properties       (prod-test PROPERTIES SUFFIX ".hx")

add_library                 (con-test SHARED ../examples/lowfive/flow-control/stateful/con-henson.cpp)
target_link_libraries       (con-test ${libraries})
set_target_properties       (con-test PROPERTIES PREFIX "")
set_target_properties       (con-test PROPERTIES SUFFIX ".hx")

add_library                 (sim-test SHARED ../examples/lowfive/flow-control/stateless/simulation.cpp)
target_link_libraries       (sim-test ${libraries})
set_target_properties       (sim-test PROPERTIES PREFIX "")
set_target_properties       (sim-test PROPERTIES SUFFIX ".hx")

add_library                 (ana-test SHARED ../examples/lowfive/flow-control/stateless/analysis.cpp)
target_link_libraries       (ana-test ${libraries})
set_target_properties       (ana-test PROPERTIES PREFIX "")
set_target_properties       (ana-test PROPERTIES SUFFIX ".hx")

add_library                 (prod-ensemble SHARED ../examples/lowfive/ensembles/producer.cpp)
target_link_libraries       (prod-ensemble ${libraries})
set_target_properties       (prod-ensemble PROPERTIES PREFIX "")
set_target_properties       (prod-ensemble PROPERTIES SUFFIX ".hx")

#add_library                 (con-ensemble SHARED ../examples/lowfive/ensembles/consumer.cpp) #stateless consumer
add_library                 (con-ensemble SHARED ../examples/lowfive/ensembles/consumer_stateful.cpp)
target_link_libraries       (con-ensemble ${libraries})
set_target_properties       (con-ensemble PROPERTIES PREFIX "")
set_target_properties       (con-ensemble PROPERTIES SUFFIX ".hx")

if                          (NOT APPLE)
   set                      (linker_flags "-pie -Wl,--export-dynamic")
   set                      (linker_flags "${linker_flags} -Wl,-u,henson_set_contexts,-u,henson_set_namemap")
else                        ()
   set                      (linker_flags "-Wl,-u,_henson_set_contexts,-u,_henson_set_namemap")
endif                       (NOT APPLE)

set_target_properties    (node0-test        PROPERTIES LINK_FLAGS ${linker_flags})
set_target_properties    (node1-test        PROPERTIES LINK_FLAGS ${linker_flags})
set_target_properties    (node2-test        PROPERTIES LINK_FLAGS ${linker_flags})
set_target_properties    (prod-test         PROPERTIES LINK_FLAGS ${linker_flags})
set_target_properties    (con-test          PROPERTIES LINK_FLAGS ${linker_flags})
set_target_properties    (sim-test          PROPERTIES LINK_FLAGS ${linker_flags})
set_target_properties    (ana-test          PROPERTIES LINK_FLAGS ${linker_flags})
set_target_properties    (prod-ensemble     PROPERTIES LINK_FLAGS ${linker_flags})
set_target_properties    (con-ensemble      PROPERTIES LINK_FLAGS ${linker_flags})

# ------------ test cycle (node 0 -> node 1 -> node 2 -> node 0) ------------------

add_test                (NAME cycle-memory
                        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test-cycle.sh ${CMAKE_CURRENT_BINARY_DIR} 0)

#add_test                (NAME cycle-passthru
#                        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test-cycle.sh ${CMAKE_CURRENT_BINARY_DIR} 1)

add_test                (NAME ensemble-fanin #Nx1
                        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test-ensemble.sh ${CMAKE_CURRENT_BINARY_DIR} 0)

add_test                (NAME ensemble-fanout #1xN
                        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test-ensemble.sh ${CMAKE_CURRENT_BINARY_DIR} 1)

add_test                (NAME ensemble-NxN #NxN
                        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test-ensemble.sh ${CMAKE_CURRENT_BINARY_DIR} 2)

#TODO: Add tests with different io frequencies (e.g., latest).

add_test                (NAME stateful-sameFile
                        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test-stateful.sh ${CMAKE_CURRENT_BINARY_DIR} 0 0 0)

add_test                (NAME stateful-differentFiles
                        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test-stateful.sh ${CMAKE_CURRENT_BINARY_DIR} 1 0 0)

add_test                (NAME stateful-differentFiles-passthru
                        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test-stateful.sh ${CMAKE_CURRENT_BINARY_DIR} 1 0 1)

add_test                (NAME stateful-subsetWriters
                        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test-stateful.sh ${CMAKE_CURRENT_BINARY_DIR} 1 1 0)

add_test                (NAME stateless-sameFile
                        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test-stateless.sh ${CMAKE_CURRENT_BINARY_DIR} 0 0)

add_test                (NAME stateless-differentFiles
                        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test-stateless.sh ${CMAKE_CURRENT_BINARY_DIR} 1 0)

add_test                (NAME stateless-differentFiles-passthru
                        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test-stateless.sh ${CMAKE_CURRENT_BINARY_DIR} 1 1)
