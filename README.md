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
CVMFS_CACHE_hdfs_CMDLINE=/usr/libexec/cvmfs_cache_hdfs/cvmfs_cache_hdfs_plugin,/foo,unix=/var/run/cvmfs/cvmfs_cache_hdfs.socket,/var/lib/cvmfs/cvmfs_cache_hdfs.log
CVMFS_CACHE_hdfs_LOCATOR=unix=/var/run/cvmfs/cvmfs_cache_hdfs.socket
```

A few notes about the command-line arguments for the HDFS plugin:

- First argument is the path to the plugin script; this script sets up environment variables for Java, then executes the underlying C++ program.
- The second argument is the base directory where cache files will be stored.
- The third argument is the *locator* line; it _must_ match `CVMFS_CACHE_hdfs_LOCATOR`.
- The fourth argument is the logfile for the cache program (it does not use the CVMFS logging infrastructure).

## Install dependencies

- `cvmfs-devel` of 2.4.0 or later.
- `bigtop-utils` for bootstrapping CVMFS environment.

