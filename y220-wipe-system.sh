#!/sbin/sh
# Runs from the CM7 updater inside TWRP. /system is already mounted UBIFS.
# Delete the stock tree without removing the mount point.
/sbin/busybox grep -q " /system " /proc/mounts || exit 1
/sbin/busybox find /system -mindepth 1 -maxdepth 1 -exec /sbin/busybox rm -rf {} \;
exit 0
