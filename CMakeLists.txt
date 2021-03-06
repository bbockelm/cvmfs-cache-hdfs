
cmake_minimum_required( VERSION 2.6 )
project( cvmfs-cache-hdfs )

set( CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake )

find_package( LibCvmfsCache REQUIRED )
find_package( Hdfs REQUIRED )
find_package( Jvm REQUIRED )
find_package( OpenSSL REQUIRED )

if( CMAKE_COMPILER_IS_GNUCXX )
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror" )
endif()

if( CMAKE_COMPILER_IS_GNUCC )
  set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror" )
endif()

SET( CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")
SET( CMAKE_MODULE_LINKER_FLAGS "-Wl,--no-undefined")
SET( CMAKE_EXE_LINKER_FLAGS "-Wl,--no-undefined")

set(SYSCONF_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/etc CACHE PATH
    "Install directory for system-wide configuration files")

add_definitions(-std=c++11)

include_directories("${PROJECT_SOURCE_DIR}" "${CVMFS_CACHE_INCLUDES}" "${HDFS_INCLUDES}" "${JVM_INCLUDES}" "${JVM_MD_INCLUDES}")

add_executable(cvmfs-cache-hdfs src/cvmfs_cache_hdfs.cc)
target_link_libraries(cvmfs-cache-hdfs "${CVMFS_CACHE_LIB}" "${HDFS_LIB}")

# These shared libraries are needed for libcvmfs_cache.a.  TODO: move this to macro file.
target_link_libraries(cvmfs-cache-hdfs pthread "${OPENSSL_LIBRARIES}")

# The plugin file requires a few customizations based on CMAKE_INSTALL_PREFIX.
configure_file(src/cvmfs_cache_hdfs_plugin.in cvmfs-cache-hdfs-plugin @ONLY)

install(
  TARGETS cvmfs-cache-hdfs
  DESTINATION libexec/cvmfs-cache-hdfs/)

install(
  PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/cvmfs-cache-hdfs-plugin
  DESTINATION libexec/cvmfs-cache-hdfs/)

install(
  FILES conf/cvmfs-cache-hdfs
  DESTINATION ${SYSCONF_INSTALL_DIR}/sysconfig/)

install(
  FILES conf/osgstorage.org.conf
  DESTINATION ${SYSCONF_INSTALL_DIR}/cvmfs/domain.d/)

install(
  FILES conf/cms.osgstorage.org.conf conf/ligo.osgstorage.org.conf
  DESTINATION ${SYSCONF_INSTALL_DIR}/cvmfs/config.d/)

install(
  FILES conf/cvmfs-cache-hdfs.conf
  DESTINATION ${SYSCONF_INSTALL_DIR}/cvmfs/)

install(
  FILES conf/cvmfs-cache-hdfs.logrotate
  DESTINATION ${SYSCONF_INSTALL_DIR}/logrotate.d
  RENAME cvmfs-cache-hdfs)

