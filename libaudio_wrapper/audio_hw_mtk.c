/*
 * audio.primary.mt6572: hands the audio HAL module over to the stock MTK
 * HAL (Y330-U01 4.2.2, /system/lib/libaudio.primary.default.so).
 *
 * MTK's libhardware loads that library itself, and its module id is
 * "libaudio". CM's hw_get_module_by_class() wants "audio" and refused it
 * (-EINVAL), so AudioFlinger had no primary output. This module carries
 * the right id and opens the device through the MTK module's methods.
 *
 * Same approach as the audio wrapper of android_device_alps_m706 (CM11).
 */

#define LOG_TAG "audio_hw_mtk"

#include <dlfcn.h>
#include <errno.h>
#include <cutils/log.h>
#include <hardware/audio.h>
#include <hardware/hardware.h>

#define MTK_AUDIO_HAL "libaudio.primary.default.so"

static int mtk_adev_open(const hw_module_t *module, const char *name,
                         hw_device_t **device)
{
    const struct hw_module_t *mtk;
    void *handle;

    handle = dlopen(MTK_AUDIO_HAL, RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL) {
        ALOGE("cannot load %s: %s", MTK_AUDIO_HAL, dlerror());
        return -EINVAL;
    }

    mtk = (const struct hw_module_t *) dlsym(handle, HAL_MODULE_INFO_SYM_AS_STR);
    if (mtk == NULL || mtk->methods == NULL || mtk->methods->open == NULL) {
        ALOGE("%s has no usable %s", MTK_AUDIO_HAL, HAL_MODULE_INFO_SYM_AS_STR);
        dlclose(handle);
        return -EINVAL;
    }

    /* The MTK device keeps this module as its hw_device_t.module. */
    return mtk->methods->open(module, name, device);
}

static struct hw_module_methods_t mtk_audio_module_methods = {
    .open = mtk_adev_open,
};

/* Not const: libhardware writes dso into it. */
struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "MTK audio HAL (Y330 stock)",
        .author = "MediaTek",
        .methods = &mtk_audio_module_methods,
    },
};
