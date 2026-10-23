/*
 * Create one dynamic UBI volume that fills an attached UBI device.
 *
 *   ubimkvol_y220 /dev/ubi0 system
 *
 * TWRP 3.1.1 for the Y220 has ubiattach but no ubimkvol. This is for
 * rebuilding system or cache from recovery after the MTD was erased:
 * erase_image <part>, attach the empty MTD (UBI writes a fresh layout),
 * create the volume, and UBIFS formats it on the first mount.
 *
 * Static, so it runs from /tmp in recovery. Not part of the ROM.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

/* Matches include/mtd/ubi-user.h on the MT6572 3.4 kernel. */
struct ubi_mkvol_req {
    int32_t vol_id;
    int32_t alignment;
    int64_t bytes;
    int8_t vol_type;
    int8_t padding1;
    int16_t name_len;
    int8_t padding2[4];
    char name[128];
} __attribute__((packed));
#define UBI_IOCMKVOL _IOW('o', 0, struct ubi_mkvol_req)
#define UBI_VOL_NUM_AUTO (-1)
#define UBI_DYNAMIC_VOLUME 3

static long read_sysfs(const char *dev, const char *attr)
{
    char path[96], buf[32];
    int fd, n;

    snprintf(path, sizeof(path), "/sys/class/ubi/%s/%s", dev, attr);
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;
    n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
        return -1;
    buf[n] = '\0';
    return strtol(buf, NULL, 10);
}

int main(int argc, char **argv)
{
    struct ubi_mkvol_req req;
    const char *dev, *base;
    long lebs, leb_size;
    int fd;

    if (argc != 3) {
        fprintf(stderr, "usage: %s /dev/ubiN name\n", argv[0]);
        return 2;
    }
    dev = argv[1];
    base = strrchr(dev, '/') ? strrchr(dev, '/') + 1 : dev;

    lebs = read_sysfs(base, "avail_eraseblocks");
    leb_size = read_sysfs(base, "eraseblock_size");
    if (lebs <= 0 || leb_size <= 0) {
        fprintf(stderr, "%s: no free eraseblocks (%ld x %ld)\n", base, lebs, leb_size);
        return 1;
    }

    memset(&req, 0, sizeof(req));
    req.vol_id = UBI_VOL_NUM_AUTO;
    req.alignment = 1;
    req.bytes = (int64_t) lebs * leb_size;
    req.vol_type = UBI_DYNAMIC_VOLUME;
    req.name_len = strlen(argv[2]);
    if (req.name_len >= (int) sizeof(req.name)) {
        fprintf(stderr, "name too long\n");
        return 2;
    }
    strcpy(req.name, argv[2]);

    fd = open(dev, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "open %s: %s\n", dev, strerror(errno));
        return 1;
    }
    if (ioctl(fd, UBI_IOCMKVOL, &req) < 0) {
        fprintf(stderr, "mkvol %s on %s: %s\n", argv[2], dev, strerror(errno));
        close(fd);
        return 1;
    }
    close(fd);
    printf("%s: volume %d \"%s\", %ld LEBs (%lld bytes)\n", dev, req.vol_id,
           argv[2], lebs, (long long) req.bytes);
    return 0;
}
