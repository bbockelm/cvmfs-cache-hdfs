
FIND_LIBRARY(CVMFS_CACHE_LIB cvmfs_cache
  HINTS
  /usr
  $ENV{CVMFS_SRC}/cvmfs
  $ENV{CVMFS_BUILD}/cvmfs
  PATH_SUFFIXES lib lib64
)

GET_FILENAME_COMPONENT(CVMFS_CACHE_LIB_DIR ${CVMFS_CACHE_LIB} PATH)

FIND_PATH(CVMFS_CACHE_INCLUDES libcvmfs_cache.h
  HINTS
  $ENV{CVMFS_SRC}/cvmfs/cache_plugin
  /usr
  PATH_SUFFIXES include
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(libcvmfs_cache DEFAULT_MSG CVMFS_CACHE_LIB CVMFS_CACHE_LIB_DIR CVMFS_CACHE_INCLUDES)

