add_library             (wilkins            comm.cpp
					    context.cpp
				            data.cpp
				            dataflow.cpp
                                            wilkins.cpp
                                            workflow.cpp)

target_link_libraries   (wilkins
                        ${libraries} ${EXTRA_FLAGS})

install                 (TARGETS wilkins
                        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/
                        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                        GROUP_READ GROUP_WRITE GROUP_EXECUTE
                        WORLD_READ WORLD_WRITE WORLD_EXECUTE)
