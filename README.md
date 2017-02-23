# CVMFS Cache Plugin for HDFS

This plugin, when enabled, provides a site-wide HDFS storage.

Example configuration:
```
CVMFS_CACHE_PRIMARY = tiered
CVMFS_CACHE_tiered_TYPE = tiered
CVMFS_CACHE_tiered_UPPER = posix
CVMFS_CACHE_posix_TYPE = posix
CVMFS_CACHE_posix_BASE = /var/lib/cvmfs
CVMFS_CACHE_posix_SHARED = true
CVMFS_CACHE_posix_QUOTA_LIMIT = 20000
CVMFS_CACHE_tiered_LOWER = hdfs
CVMFS_CACHE_hdfs_TYPE = external
CVMFS_CACHE_hdfs_CMDLINE = /usr/bin/cvmfs_cache_hdfs
```

