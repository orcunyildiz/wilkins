project                     (Wilkins)
cmake_minimum_required      (VERSION 3.9)

option                      (lowfive            "Build Wilkins with Lowfive"  OFF)

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
  include_directories         (${LOWFIVE_INCLUDE_DIR})
  set                         (libraries ${libraries} ${LOWFIVE_LIBRARY})
  # DIY
  option                      (build_examples "" OFF)
  option                      (build_tests "" OFF)
  option                      (python "" OFF)
  add_subdirectory            (ext/diy)
  set                         (libraries ${libraries} diy)

  # fmt
  option                      (FMT_INSTALL "" ON)
  add_subdirectory            (ext/fmt)
  set                         (libraries ${libraries} fmt::fmt)

  # HDF5
  if                          (HDF5_LIBRARY AND Z_LIBRARY)
    message                   ("HDF5_LIBRARY = " ${HDF5_LIBRARY})
    message                   ("Z_LIBRARY = " ${Z_LIBRARY})
    set                       (HDF5_LIBRARY ${HDF5_LIBRARY} ${Z_LIBRARY})
  else                        ()
    message                   ("Looking for HDF5")
    set                       (HDF5_USE_STATIC_LIBRARIES ON)
    find_package              (HDF5 REQUIRED)
    message                   ("HDF5_C_LIBRARY_hdf5 = " ${HDF5_C_LIBRARY_hdf5})
    message                   ("HDF5_C_LIBRARY_z = " ${HDF5_C_LIBRARY_z})
    set                       (HDF5_LIBRARY ${HDF5_C_LIBRARY_hdf5} ${HDF5_C_LIBRARY_z})
  endif                       ()
  set                         (libraries ${libraries} ${HDF5_LIBRARY})
  include_directories         (${HDF5_INCLUDE_DIR})
endif                       (lowfive)


# Boost
find_package                (Boost 1.59.0 COMPONENTS serialization REQUIRED)
message                     (STATUS "Boost libraries: " ${Boost_LIBRARIES})

# Set include directories
set                         (CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-isystem")
include_directories         (${Boost_INCLUDE_DIRS}
                             ${CMAKE_CURRENT_BINARY_DIR}
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

