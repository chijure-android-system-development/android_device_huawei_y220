/*
 * GraphicBufferMapper::getIonFd from MTK's libui, used by the stock camera
 * client (libcam.client.so) and hwcomposer.mt6572.so.
 *
 * MTK's GraphicBufferMapper opens a second gralloc device, "extra", next to
 * the module, and getIonFd is the first call after its hw_device_t. CM's
 * GraphicBufferMapper has no such member, so the device is opened here once
 * per process. The this pointer is CM's object and is not used.
 */

#define LOG_TAG "mtk_symbols"

#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <cutils/log.h>
#include <hardware/gralloc.h>

struct gralloc_extra_device {
    struct hw_device_t common;
    int (*getIonFd)(struct gralloc_extra_device *dev, buffer_handle_t handle,
                    int *idx, int *num);
};

static struct gralloc_extra_device *extra_dev;
static pthread_once_t extra_once = PTHREAD_ONCE_INIT;

static void open_extra(void)
{
    const hw_module_t *module;
    hw_device_t *dev = NULL;

    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module) != 0) {
        ALOGE("getIonFd: no gralloc module");
        return;
    }
    if (module->methods->open(module, "extra", &dev) != 0 || dev == NULL) {
        ALOGE("getIonFd: gralloc has no extra device");
        return;
    }
    extra_dev = (struct gralloc_extra_device *) dev;
}

namespace android {

class GraphicBufferMapper {
public:
    int getIonFd(buffer_handle_t handle, int *idx, int *num);
};

int GraphicBufferMapper::getIonFd(buffer_handle_t handle, int *idx, int *num)
{
    int err;

    pthread_once(&extra_once, open_extra);
    if (extra_dev == NULL)
        return -EINVAL;

    err = extra_dev->getIonFd(extra_dev, handle, idx, num);
    if (err)
        ALOGW("getIonFd(...) failed %d (%s)", err, strerror(-err));
    return err;
}

}; // namespace android
