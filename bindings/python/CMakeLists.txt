list                        (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
find_package                (MPI4Py)

pybind11_add_module         (pywilkins py-wilkins.cpp)
target_link_libraries       (pywilkins PRIVATE ${libraries} wilkins)

if                          (MPI4Py_FOUND)
    target_compile_definitions  (pywilkins PUBLIC "-DWILKINS_MPI4PY")
    target_include_directories  (pywilkins PUBLIC ${MPI4Py_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include ${HDF5_INCLUDE_DIR})
endif()

list                        (APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/ext/pybind11/tools")
find_package                (PythonLibsNew)
string                      (REPLACE ${PYTHON_PREFIX}/ "" PYTHON_SITE_PACKAGES_RELATIVE ${PYTHON_SITE_PACKAGES})

install                     (TARGETS pywilkins
                             LIBRARY DESTINATION ${PYTHON_SITE_PACKAGES_RELATIVE}/wilkins)

install                     (FILES wilkins/utils.py
                             DESTINATION ${PYTHON_SITE_PACKAGES_RELATIVE}/wilkins)

install                     (TARGETS pywilkins
                            DESTINATION ${CMAKE_INSTALL_PREFIX}/examples/python
                            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_WRITE GROUP_EXECUTE
                            WORLD_READ WORLD_WRITE WORLD_EXECUTE)
