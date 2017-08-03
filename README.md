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
# Second argument should be the prefix of the cache in HDFS and should be changed.
CVMFS_CACHE_hdfs_CMDLINE=/usr/libexec/cvmfs_cache_hdfs/cvmfs_cache_hdfs_plugin,/foo,unix=/var/run/cvmfs/cvmfs_cache_hdfs.socket
CVMFS_CACHE_hdfs_LOCATOR=unix=/var/run/cvmfs/cvmfs_cache_hdfs.socket
```

## Install dependencies

- `cvmfs-devel` of 2.4.0 or later.
- `bigtop-utils` for bootstrapping CVMFS environment.

