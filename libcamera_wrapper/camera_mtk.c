/*
 * camera.mt6572: hands the camera HAL module over to the stock MTK HAL
 * (Y330-U01 4.2.2, /system/lib/hw/camera.mtk.so) with two changes for the
 * Y220's single gc0329:
 *
 * - The MTK HAL always adds the analog TV input as a second back camera
 *   (SENSOR_ATV). Opening it gives no frames, and the Camera app picked it.
 *   Only the main sensor, camera 0, is reported.
 * - Its scene modes include "hdr", so the Camera app offered HDR. The
 *   gc0329 has none: "hdr" is taken out of scene-mode-values.
 * - The JPEG of a capture comes as MTK_CAMERA_MSG_EXT_DATA (an MTK message
 *   that MTK's CameraClient unpacked). CM's CameraClient dropped it as an
 *   unwanted message and the app never got the picture. It is passed on
 *   as CAMERA_MSG_COMPRESSED_IMAGE.
 */

#define LOG_TAG "camera_mtk"

#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <cutils/log.h>
#include <hardware/camera.h>
#include <hardware/hardware.h>

#define MTK_CAMERA_HAL "/system/lib/hw/camera.mtk.so"

/* MTK's extended data message; the payload starts with an MTK header. */
#define MTK_CAMERA_MSG_EXT_DATA 0x80000000
/* The JPEG starts within this many bytes of the payload. */
#define MTK_EXT_HEADER_MAX 64

static camera_module_t *mtk;
static camera_device_ops_t ops;
static char *(*mtk_get_parameters)(struct camera_device *);
static void (*mtk_set_callbacks)(struct camera_device *, camera_notify_callback,
                                 camera_data_callback, camera_data_timestamp_callback,
                                 camera_request_memory, void *);

/* The app's callbacks; one camera device is open at a time. */
static camera_data_callback app_data_cb;
static camera_request_memory app_get_memory;

static int load_mtk(void)
{
    void *handle;

    if (mtk != NULL)
        return 0;
    handle = dlopen(MTK_CAMERA_HAL, RTLD_NOW);
    if (handle == NULL) {
        ALOGE("cannot load %s: %s", MTK_CAMERA_HAL, dlerror());
        return -EINVAL;
    }
    mtk = (camera_module_t *) dlsym(handle, HAL_MODULE_INFO_SYM_AS_STR);
    if (mtk == NULL) {
        ALOGE("%s has no %s", MTK_CAMERA_HAL, HAL_MODULE_INFO_SYM_AS_STR);
        dlclose(handle);
        return -EINVAL;
    }
    return 0;
}

/* Drops "hdr" from the scene-mode-values list, in place. */
static void drop_hdr(char *params)
{
    char *list, *end, *hdr;
    size_t len;

    list = strstr(params, "scene-mode-values=");
    if (list == NULL)
        return;
    list += strlen("scene-mode-values=");
    end = strchr(list, ';');
    if (end == NULL)
        end = list + strlen(list);

    for (hdr = list; hdr < end; hdr++) {
        if (strncmp(hdr, "hdr", 3) != 0)
            continue;
        if ((hdr != list && hdr[-1] != ',') || (hdr[3] != ',' && hdr + 3 != end))
            continue;
        /* Take the item and one comma with it. */
        len = 3;
        if (hdr[3] == ',')
            len++;
        else if (hdr != list)
            hdr--, len++;
        memmove(hdr, hdr + len, strlen(hdr + len) + 1);
        return;
    }
}

static char *wrap_get_parameters(struct camera_device *dev)
{
    char *params = mtk_get_parameters(dev);

    if (params != NULL)
        drop_hdr(params);
    return params;
}

static void wrap_data_cb(int32_t msg_type, const camera_memory_t *data,
                         unsigned int index, camera_frame_metadata_t *metadata,
                         void *user)
{
    const unsigned char *p;
    camera_memory_t *jpeg;
    size_t off;

    if ((uint32_t) msg_type != MTK_CAMERA_MSG_EXT_DATA || data == NULL ||
        data->data == NULL || app_get_memory == NULL) {
        app_data_cb(msg_type, data, index, metadata, user);
        return;
    }

    /* Only the compressed image is wanted; other MTK data has no JPEG. */
    p = data->data;
    for (off = 0; off + 3 <= data->size && off < MTK_EXT_HEADER_MAX; off++)
        if (p[off] == 0xff && p[off + 1] == 0xd8 && p[off + 2] == 0xff)
            break;
    if (off + 3 > data->size || off >= MTK_EXT_HEADER_MAX)
        return;

    jpeg = app_get_memory(-1, data->size - off, 1, user);
    if (jpeg == NULL || jpeg->data == NULL) {
        ALOGE("no memory for a %u byte JPEG", (unsigned) (data->size - off));
        return;
    }
    memcpy(jpeg->data, p + off, data->size - off);
    app_data_cb(CAMERA_MSG_COMPRESSED_IMAGE, jpeg, 0, NULL, user);
    jpeg->release(jpeg);
}

static void wrap_set_callbacks(struct camera_device *dev,
                               camera_notify_callback notify_cb,
                               camera_data_callback data_cb,
                               camera_data_timestamp_callback data_cb_timestamp,
                               camera_request_memory get_memory, void *user)
{
    app_data_cb = data_cb;
    app_get_memory = get_memory;
    mtk_set_callbacks(dev, notify_cb, data_cb != NULL ? wrap_data_cb : NULL,
                      data_cb_timestamp, get_memory, user);
}

static int wrap_get_number_of_cameras(void)
{
    if (load_mtk() != 0)
        return 0;
    return mtk->get_number_of_cameras() > 0 ? 1 : 0;
}

static int wrap_get_camera_info(int camera_id, struct camera_info *info)
{
    if (load_mtk() != 0 || camera_id != 0)
        return -EINVAL;
    return mtk->get_camera_info(camera_id, info);
}

static int wrap_open(const hw_module_t *module, const char *id, hw_device_t **device)
{
    camera_device_t *dev;
    int ret;

    if (load_mtk() != 0 || strcmp(id, "0") != 0)
        return -EINVAL;

    ret = mtk->common.methods->open(&mtk->common, id, device);
    if (ret != 0)
        return ret;

    dev = (camera_device_t *) *device;
    if (mtk_get_parameters == NULL) {
        ops = *dev->ops;
        mtk_get_parameters = ops.get_parameters;
        ops.get_parameters = wrap_get_parameters;
        mtk_set_callbacks = ops.set_callbacks;
        ops.set_callbacks = wrap_set_callbacks;
    }
    dev->ops = &ops;
    return 0;
}

static struct hw_module_methods_t camera_module_methods = {
    .open = wrap_open,
};

/* Not const: libhardware writes dso into it. */
camera_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = CAMERA_MODULE_API_VERSION_1_0,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = CAMERA_HARDWARE_MODULE_ID,
        .name = "MTK camera HAL (Y330 stock), Y220 main sensor only",
        .author = "MediaTek",
        .methods = &camera_module_methods,
    },
    .get_number_of_cameras = wrap_get_number_of_cameras,
    .get_camera_info = wrap_get_camera_info,
};
