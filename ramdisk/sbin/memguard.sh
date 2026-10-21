#!/sbin/sh
# UBIFS kmallocs a LEB-sized buffer (~248 KB, orph_buf and lpt_buf) on every
# mount. Backup and restore stream hundreds of MB through the page cache and
# leave no such contiguous block, so the next mount fails with ENOMEM
# ("Failed to mount '/system' (Out of memory)"). Keep the clean page cache
# small so the buddy allocator can merge free pages again.
while true; do
    cached=$(awk '/^Cached:/ {print $2}' /proc/meminfo)
    if [ "${cached:-0}" -gt 32768 ]; then
        echo 3 > /proc/sys/vm/drop_caches
    fi
    sleep 2
done
