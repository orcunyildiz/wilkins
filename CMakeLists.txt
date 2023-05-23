project                     (Wilkins)
cmake_minimum_required      (VERSION 3.9)

option                      (lowfive                   "Build Wilkins with Lowfive"         ON)
option                      (wilkins_python            "Build Wilkins with python bindings" ON)

set                         (CMAKE_POSITION_INDEPENDENT_CODE ON)

# C++17
if                          (NOT DEFINED CMAKE_CXX_STANDARD)
    set                     (CMAKE_CXX_STANDARD 17)
endif                       ()

# Threads
find_package                (Threads)

# MPI
find_package              (MPI REQUIRED)
set                       (libraries MPI::MPI_C)

# DIY
option                      (build_examples "" OFF)
option                      (build_tests "" OFF)
add_subdirectory            (ext/diy)
set                         (libraries ${libraries} diy)

# fmt
option                      (FMT_INSTALL "" OFF)
add_subdirectory            (ext/fmt)
set                         (libraries ${libraries} fmt::fmt)

#henson
if                          (HENSON_LIBRARY AND HENSON_PMPI_LIBRARY)
  message                  ("HENSON_LIBRARY = " ${HENSON_LIBRARY})
  message                  ("HENSON_PMPI_LIBRARY = " ${HENSON_PMPI_LIBRARY})
  set                      (HENSON_LIBRARIES ${HENSON_PMPI_LIBRARY} ${HENSON_LIBRARY})
else                        ()
  message                   ("Looking for HENSON")
  find_library              (HENSON_LIBRARY henson)
  find_library              (HENSON_PMPI_LIBRARY henson-pmpi)
  message                   ("found HENSON_LIBRARY = " ${HENSON_LIBRARY})
  message                   ("found HENSON_PMPI_LIBRARY = " ${HENSON_PMPI_LIBRARY})
  set                       (HENSON_LIBRARIES ${HENSON_PMPI_LIBRARY} ${HENSON_LIBRARY})
endif                       ()
set                         (libraries ${libraries} ${HENSON_LIBRARIES})
include_directories         (${HENSON_INCLUDE_DIR})

#lowfive
if                          (lowfive)
  if                          (LOWFIVE_LIBRARY AND LOWFIVE_DIST_LIBRARY)
    message                   ("LOWFIVE_LIBRARY = " ${LOWFIVE_LIBRARY})
    message                   ("LOWFIVE_DIST = " ${LOWFIVE_DIST_LIBRARY})
    set                       (LOWFIVE_LIBRARIES ${LOWFIVE_LIBRARY} ${LOWFIVE_DIST_LIBRARY})
  else                      ()
    message                   ("Looking for LOWFIVE")
    find_library              (LOWFIVE_LIBRARY lowfive)
    find_library              (LOWFIVE_DIST_LIBRARY lowfive-dist)
    message                   ("found LOWFIVE_LIBRARY = " ${LOWFIVE_LIBRARY})
    message                   ("found LOWFIVE_DIST = " ${LOWFIVE_DIST_LIBRARY})
    set                       (LOWFIVE_LIBRARIES ${LOWFIVE_LIBRARY} ${LOWFIVE_DIST_LIBRARY})
  endif                     ()
  set                       (libraries ${libraries} ${LOWFIVE_LIBRARIES})
  include_directories       (${LOWFIVE_INCLUDE_DIR})

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

#YAML-cpp
add_subdirectory            (ext/yaml-cpp)
set                         (libraries ${libraries} yaml-cpp::yaml-cpp)

# Set include directories
set                         (CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-isystem")
include_directories         (${CMAKE_CURRENT_SOURCE_DIR}/include
                             SYSTEM ${MPI_INCLUDE_PATH})

# Set libraries
set                         (libraries
                             ${libraries}
                             ${CMAKE_THREAD_LIBS_INIT}
                             ${CMAKE_DL_LIBS})

# subdirectories
add_subdirectory            (src)
add_subdirectory            (examples)

#python bindings
if                          (wilkins_python)
    add_subdirectory        (ext/pybind11)
    add_subdirectory        (bindings/python)
endif                       (wilkins_python)

#testing
enable_testing              ()
include                     (CTest)
add_subdirectory            (tests)
