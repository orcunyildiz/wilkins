project                     (Wilkins)
cmake_minimum_required      (VERSION 3.9)

option                      (lowfive                   "Build Wilkins with Lowfive"         ON)
option                      (wilkins_python            "Build Wilkins with python bindings" ON)

set                         (CMAKE_POSITION_INDEPENDENT_CODE ON)

# C++14
set                         (CMAKE_CXX_STANDARD        14)
set                         (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

# Threads
find_package                (Threads)

# MPI
find_package              (MPI REQUIRED)
set                       (libraries    ${libraries} ${MPI_C_LIBRARIES} ${MPI_CXX_LIBRARIES})

#lowfive
if                          (lowfive)
  add_subdirectory            (ext/lowfive)
  set                         (libraries ${libraries} lowfive lowfive-dist)

  # HDF5
  if                          (HDF5_LIBRARY AND HDF5_HL_LIBRARY AND Z_LIBRARY)
    message                   ("HDF5_LIBRARY = " ${HDF5_LIBRARY})
    message                   ("HDF5_HL_LIBRARY = " ${HDF5_HL_LIBRARY})
    message                   ("Z_LIBRARY = " ${Z_LIBRARY})
    set                       (HDF5_LIBRARY ${HDF5_HL_LIBRARY} ${HDF5_LIBRARY} ${Z_LIBRARY})
  else                        ()
    message                   ("Looking for HDF5")
    if                        (NOT APPLE)
      set                       (HDF5_USE_STATIC_LIBRARIES ON)
    endif                     ()
    find_package              (HDF5 REQUIRED COMPONENTS C HL)
    message                   ("HDF5_C_LIBRARIES = " ${HDF5_C_LIBRARIES})
    message                   ("HDF5_HL_LIBRARIES = " ${HDF5_HL_LIBRARIES})
    message                   ("HDF5_C_LIBRARY_hdf5 = " ${HDF5_C_LIBRARY_hdf5})
    message                   ("HDF5_C_LIBRARY_z = " ${HDF5_C_LIBRARY_z})
    set                       (HDF5_LIBRARY ${HDF5_C_LIBRARIES} ${HDF5_HL_LIBRARIES} ${HDF5_C_LIBRARY_hdf5} ${HDF5_C_LIBRARY_z})
  endif                       ()
  set                         (libraries ${libraries} ${HDF5_LIBRARY})
  include_directories         (${HDF5_INCLUDE_DIR})
endif                       (lowfive)

#henson
if                          (HENSON_LIBRARY AND HENSON_PMPI_LIBRARY)
  message                  ("HENSON_LIBRARY = " ${HENSON_LIBRARY})
  message                  ("HENSON_PMPI_LIBRARY = " ${HENSON_PMPI_LIBRARY})
  set                      (HENSON_LIBRARIES ${HENSON_PMPI_LIBRARY} ${HENSON_LIBRARY})
else                        ()
  message                   ("Looking for HENSON")
  find_library              (HENSON_LIBRARY henson)
  find_library              (HENSON_PMPI_LIBRARY henson-pmpi)
  message                   ("HENSON_LIBRARY = " ${HENSON_LIBRARY})
  message                   ("HENSON_PMPI_LIBRARY = " ${HENSON_PMPI_LIBRARY})
  set                       (HENSON_LIBRARIES ${HENSON_PMPI_LIBRARY} ${HENSON_LIBRARY})
endif                       ()
set                         (libraries ${libraries} ${HENSON_LIBRARIES})
include_directories         (${HENSON_INCLUDE_DIR})

#YAML-cpp
add_subdirectory            (ext/yaml-cpp)
set                         (libraries ${libraries} yaml-cpp::yaml-cpp)

# Set include directories
set                         (CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-isystem")
include_directories         (${CMAKE_CURRENT_BINARY_DIR}
                             ${CMAKE_CURRENT_SOURCE_DIR}
                             ${CMAKE_CURRENT_SOURCE_DIR}/include
                             SYSTEM ${MPI_INCLUDE_PATH})

# Set libraries
set                         (libraries
                             ${libraries}
                             ${CMAKE_THREAD_LIBS_INIT}
                             ${CMAKE_DL_LIBS})

set (CMAKE_LINKER_FLAGS ${CMAKE_LINKER_FLAGS} "-Wl,--export-dynamic -dynamic")

# subdirectories
add_subdirectory            (src)
add_subdirectory            (examples)

#python bindings
if                          (wilkins_python)
    add_subdirectory        (bindings/python)
endif                       (wilkins_python)
