pybind11_add_module         (pywilkins py-wilkins.cpp)
target_link_libraries       (pywilkins PRIVATE ${libraries} wilkins)

install                     (TARGETS pywilkins
                            DESTINATION ${CMAKE_INSTALL_PREFIX}/examples/python
                            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_WRITE GROUP_EXECUTE
                            WORLD_READ WORLD_WRITE WORLD_EXECUTE)
