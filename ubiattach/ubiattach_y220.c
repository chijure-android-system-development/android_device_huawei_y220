/*
 * Attach the Y220's UBI partitions before init mounts them.
 *
 * system, userdata and cache are UBIFS on NAND. The stock MTK init does the
 * attach inside "mount ubifs ubi@<name>"; CM's init does not know ubi@.
 * init.mt6572.rc runs this with exec in "on fs" and then mounts
 * /dev/ubiN_0. The UBI numbers are fixed so the paths match TWRP's
 * recovery.fstab: ubi0 = system, ubi1 = userdata, ubi2 = cache.
 *
 * Static: it runs before /system is mounted.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#define LOG_TAG "ubiattach_y220"
#include <cutils/klog.h>

/* Matches include/mtd/ubi-user.h on the MT6572 3.4 kernel. */
struct ubi_attach_req {
    int32_t ubi_num;
    int32_t mtd_num;
    int32_t vid_hdr_offset;
    int8_t padding[12];
};
#define UBI_IOCATT _IOW('o', 64, struct ubi_attach_req)

#define UBI_CTRL "/dev/ubi_ctrl"

static const char *parts[] = { "system", "userdata", "cache" };

static int mtd_by_name(const char *name)
{
    char line[128], want[32];
    FILE *f;
    int num = -1, n;

    f = fopen("/proc/mtd", "r");
    if (!f)
        return -1;
    snprintf(want, sizeof(want), "\"%s\"", name);
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, want) && sscanf(line, "mtd%d:", &n) == 1) {
            num = n;
            break;
        }
    }
    fclose(f);
    return num;
}

/* ueventd normally makes the node; make it from sysfs if it has not yet. */
static int open_ctrl(void)
{
    char buf[16];
    int fd, major, minor;

    if (access(UBI_CTRL, F_OK) < 0) {
        fd = open("/sys/class/misc/ubi_ctrl/dev", O_RDONLY);
        if (fd < 0)
            return -1;
        memset(buf, 0, sizeof(buf));
        read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (sscanf(buf, "%d:%d", &major, &minor) != 2)
            return -1;
        mknod(UBI_CTRL, S_IFCHR | 0600, makedev(major, minor));
    }
    return open(UBI_CTRL, O_RDONLY);
}

int main(void)
{
    struct ubi_attach_req req;
    char path[64];
    unsigned i;
    int ctrl, mtd, ret = 0;

    klog_init();
    klog_set_level(6);

    ctrl = open_ctrl();
    if (ctrl < 0) {
        KLOG_ERROR(LOG_TAG, "cannot open " UBI_CTRL ": %s\n", strerror(errno));
        return 1;
    }

    for (i = 0; i < sizeof(parts) / sizeof(parts[0]); i++) {
        snprintf(path, sizeof(path), "/sys/class/ubi/ubi%u", i);
        if (access(path, F_OK) == 0)
            continue;

        mtd = mtd_by_name(parts[i]);
        if (mtd < 0) {
            KLOG_ERROR(LOG_TAG, "no MTD partition named %s\n", parts[i]);
            ret = 1;
            continue;
        }

        memset(&req, 0, sizeof(req));
        req.ubi_num = i;
        req.mtd_num = mtd;
        if (ioctl(ctrl, UBI_IOCATT, &req) < 0) {
            KLOG_ERROR(LOG_TAG, "attach mtd%d (%s) as ubi%u: %s\n",
                       mtd, parts[i], i, strerror(errno));
            ret = 1;
            continue;
        }
        KLOG_INFO(LOG_TAG, "mtd%d (%s) is ubi%u\n", mtd, parts[i], i);
    }

    close(ctrl);
    return ret;
}
