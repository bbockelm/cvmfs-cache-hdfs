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
CVMFS_CACHE_hdfs_CMDLINE=/usr/libexec/cvmfs-cache-hdfs/cvmfs-cache_hdfs_plugin,/etc/cvmfs/cvmfs-cache-hdfs.conf
CVMFS_CACHE_hdfs_LOCATOR=unix=/var/run/cvmfs/cvmfs-cache_hdfs.socket
```

A few notes about the command-line arguments for the HDFS plugin:

- First argument is the path to the plugin script; this script sets up environment variables for Java, then executes the underlying C++ program.
  You can override the environment variables set here by making additions to `/etc/sysconfig/cvmfs-cache-hdfs`.
- The second argument (required) is the config file location.
- Additional arguments are key=value pairs that are overrides to configuration file settings.

The `LOCATOR` configuration variable for the CVMFS config MUST have the same value as the plugin's config file.

## Install dependencies

- `cvmfs-devel` of 2.4.0 or later.
- `bigtop-utils` for bootstrapping CVMFS environment.

