
cmake_minimum_required( VERSION 2.6 )
project( cvmfs-cache-hdfs )

set( CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake )

find_package( LibCvmfsCache REQUIRED )
find_package( Hdfs REQUIRED )
find_package( Jvm REQUIRED )

if( CMAKE_COMPILER_IS_GNUCXX )
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror" )
endif()

if( CMAKE_COMPILER_IS_GNUCC )
  set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror" )
endif()

SET( CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")
SET( CMAKE_MODULE_LINKER_FLAGS "-Wl,--no-undefined")
SET( CMAKE_EXE_LINKER_FLAGS "-Wl,--no-undefined")

add_definitions(-std=c++11)

include_directories("${PROJECT_SOURCE_DIR}" "${CVMFS_CACHE_INCLUDES}" "${HDFS_INCLUDES}" "${JVM_INCLUDES}" "${JVM_MD_INCLUDES}")

add_executable(cvmfs_cache_hdfs src/cvmfs_cache_hdfs.cc)
target_link_libraries(cvmfs_cache_hdfs ${CVMFS_CACHE_LIB})

install(
  PROGRAMS cvmfs_cache_hdfs
  DESTINATION bin)

