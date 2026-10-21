/*
 * C++ symbols that the stock MT6572 media blobs take from the MTK
 * libbinder and libstagefright.
 */
#include <unistd.h>
#include <binder/MemoryHeapBase.h>

namespace android {

/*
 * libstagefrighthw wraps buffers it has already mapped (PMEM). Stock
 * MemoryHeapBaseMTK(fd, base, size, flags, device) is MemoryHeapBase()
 * plus init(dup(fd), ...), and adds no members; init leaves mNeedUnmap
 * false, so the heap never unmaps base.
 */
class MemoryHeapBaseMTK : public MemoryHeapBase {
public:
    MemoryHeapBaseMTK(int fd, void *base, int size, int flags,
            const char *device);
};

MemoryHeapBaseMTK::MemoryHeapBaseMTK(int fd, void *base, int size,
        int flags, const char *device)
    : MemoryHeapBase()
{
    init(dup(fd), base, size, flags, device);
}

} // namespace android

/*
 * libMtkOmxVdec asks the MTK AwesomePlayer for the playback clock to drop
 * late frames. The CM7 AwesomePlayer never hands its pointer to the
 * decoder; answer -1, which stock returns while seeking or paused, so
 * the decoder drops nothing.
 */
extern "C" void _ZN7android13AwesomePlayer24mtk_omx_get_current_timeEPx(
        void *player, long long *time_us)
{
    (void)player;
    *time_us = -1;
}
