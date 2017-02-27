# CVMFS Cache Plugin for HDFS

This plugin, when enabled, provides a site-wide HDFS storage.

Example configuration:
```
CVMFS_CACHE_PRIMARY=tiered
CVMFS_CACHE_tiered_TYPE=tiered
CVMFS_CACHE_tiered_UPPER=posix
CVMFS_CACHE_posix_TYPE=posix
CVMFS_CACHE_posix_SHARED=yes
CVMFS_CACHE_tiered_LOWER=hdfs
CVMFS_CACHE_hdfs_TYPE=external
# Multiple arguments must be comma-separated.
CVMFS_CACHE_hdfs_CMDLINE=/usr/bin/cvmfs_cache_hdfs,/foo,/var/run/cvmfs/cvmfs_cache_hdfs.socket
CVMFS_CACHE_hdfs_LOCATOR=/var/run/cvmfs/cvmfs_cache_hdfs.socket
```

