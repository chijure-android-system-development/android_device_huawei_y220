/*
 * Symbols that the stock MT6572 blobs take from the MTK bionic and the
 * CM7 one does not have. mediaserver gets this library with LD_PRELOAD,
 * so the GB linker finds them while it relocates the video codec libs
 * (libsched, libvcodec_utility, libvcodecdrv, libMtkOmxVdec).
 */
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "pthread_internal.h"

/* cpu_set_t is opaque to the callers; the kernel takes a byte mask. */
int sched_setaffinity(pid_t pid, size_t setsize, const void *set)
{
    return syscall(__NR_sched_setaffinity, pid, setsize, set);
}

/* The syscall returns the bytes it wrote; glibc and MTK bionic return 0. */
int sched_getaffinity(pid_t pid, size_t setsize, void *set)
{
    int ret = syscall(__NR_sched_getaffinity, pid, setsize, set);

    if (ret < 0)
        return ret;
    if ((size_t)ret < setsize)
        memset((char *)set + ret, 0, setsize - ret);
    return 0;
}

pid_t __pthread_gettid(pthread_t thid)
{
    return ((pthread_internal_t *)thid)->kernel_id;
}
