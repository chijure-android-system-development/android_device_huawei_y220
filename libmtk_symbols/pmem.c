/*
 * MTK's libcutils pmem helpers, used by the stock libdpframework.so (camera
 * and MDP). Same code as the CM7 tree's libcutils/pmem.c.
 */

#define LOG_TAG "mtk_symbols"

#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/android_pmem.h>

#include <cutils/log.h>

#define PMEM_DEVICE "/dev/pmem_multimedia"

static size_t page_align(size_t size)
{
    size_t page = getpagesize();
    return (size + page - 1) & ~(page - 1);
}

static void* alloc_flags(size_t size, int *pfd, int flags)
{
    size_t len = page_align(size);
    void *base;
    int fd;

    fd = open(PMEM_DEVICE, flags);
    if (fd < 0) {
        ALOGE("open %s failed", PMEM_DEVICE);
        return NULL;
    }

    base = mmap(0, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED) {
        ALOGE("mmap %u bytes failed", len);
        close(fd);
        return NULL;
    }

    if (pfd)
        *pfd = fd;
    return base;
}

void* pmem_alloc(size_t size, int *pfd)
{
    return alloc_flags(size, pfd, O_RDWR);
}

void* pmem_alloc_sync(size_t size, int *pfd)
{
    return alloc_flags(size, pfd, O_RDWR | O_SYNC);
}

int pmem_free(void *ptr, size_t size, int fd)
{
    int err = munmap(ptr, page_align(size));

    close(fd);
    return err ? -1 : 0;
}

void* pmem_get_phys(int fd)
{
    struct pmem_region region;

    if (ioctl(fd, PMEM_GET_PHYS, &region) < 0) {
        ALOGE("PMEM_GET_PHYS failed on fd %d", fd);
        return NULL;
    }
    return (void *) region.offset;
}
