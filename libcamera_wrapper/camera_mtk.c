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
 * - It offers picture sizes up to 2560x1920, interpolated from the VGA
 *   sensor. Only sizes within 640x480 are kept, and a larger picture-size
 *   is lowered to 640x480.
 * - The JPEG of a capture comes as MTK_CAMERA_MSG_EXT_DATA (an MTK message
 *   that MTK's CameraClient unpacked). CM's CameraClient dropped it as an
 *   unwanted message and the app never got the picture. It is passed on
 *   as CAMERA_MSG_COMPRESSED_IMAGE.
 * - If preview is stopped while a capture is still in the HAL (shutter,
 *   then leave the app at once), the capture thread never finishes and
 *   stopPreview waits for it forever: the app gets an ANR and the camera
 *   stays open until mediaserver restarts. stop_preview and release
 *   wait first for the JPEG (or an error) of a capture in progress.
 * - Video: the HAL names its recording frames "yuv420i-yyuvyy-3plane"
 *   (MTK's I420: Y, U, V planes). CM's CameraSource only knows the AOSP
 *   names and aborted mediaserver on it. get_parameters reports it as
 *   "yuv420p", which CameraSource maps to OMX YUV420Planar, the same
 *   layout; set_parameters gives the HAL its own name back. Metadata
 *   buffers are refused: they would carry MTK buffer handles that the
 *   software encoders (OMX.google.*) cannot read.
 */

#define LOG_TAG "camera_mtk"

#include <dlfcn.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
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
/* A VGA capture takes well under a second; give up waiting after this. */
#define CAPTURE_TIMEOUT_S 5

static camera_module_t *mtk;
static camera_device_ops_t ops;
static char *(*mtk_get_parameters)(struct camera_device *);
static void (*mtk_set_callbacks)(struct camera_device *, camera_notify_callback,
                                 camera_data_callback, camera_data_timestamp_callback,
                                 camera_request_memory, void *);
static int (*mtk_take_picture)(struct camera_device *);
static void (*mtk_stop_preview)(struct camera_device *);
static void (*mtk_release)(struct camera_device *);
static int (*mtk_set_parameters)(struct camera_device *, const char *);

/* The app's callbacks; one camera device is open at a time. */
static camera_data_callback app_data_cb;
static camera_request_memory app_get_memory;
static camera_notify_callback app_notify_cb;

/* Set from take_picture until its JPEG or an error comes back. */
static pthread_mutex_t capture_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t capture_done = PTHREAD_COND_INITIALIZER;
static int capturing;

static void end_capture(void)
{
    pthread_mutex_lock(&capture_lock);
    capturing = 0;
    pthread_cond_broadcast(&capture_done);
    pthread_mutex_unlock(&capture_lock);
}

static void wait_capture(void)
{
    struct timespec ts;
    int ret = 0;

    pthread_mutex_lock(&capture_lock);
    if (capturing) {
        ALOGW("capture in progress, waiting before stopping preview");
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += CAPTURE_TIMEOUT_S;
        while (capturing && ret != ETIMEDOUT)
            ret = pthread_cond_timedwait(&capture_done, &capture_lock, &ts);
        if (capturing)
            ALOGE("capture did not finish in %d s", CAPTURE_TIMEOUT_S);
        capturing = 0;
    }
    pthread_mutex_unlock(&capture_lock);
}

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

/* The gc0329 is VGA; bigger pictures are only interpolated. */
#define MAX_PICTURE_W 640
#define MAX_PICTURE_H 480

static int size_fits(const char *size)
{
    int w, h;

    return sscanf(size, "%dx%d", &w, &h) == 2 && w <= MAX_PICTURE_W && h <= MAX_PICTURE_H;
}

/* Value of "key=" in params, and where it ends. NULL if absent. */
static char *find_value(char *params, const char *key, char **end)
{
    size_t len = strlen(key);
    char *v = params;

    while ((v = strstr(v, key)) != NULL) {
        if ((v == params || v[-1] == ';') && v[len] == '=')
            break;
        v += len;
    }
    if (v == NULL)
        return NULL;
    v += len + 1;
    *end = strchr(v, ';');
    if (*end == NULL)
        *end = v + strlen(v);
    return v;
}

/* Keeps the picture sizes within the sensor and lowers picture-size. */
static void limit_picture_size(char *params)
{
    char *list, *end, *item, *next, *out;
    size_t len;

    list = find_value(params, "picture-size-values", &end);
    if (list != NULL) {
        out = list;
        for (item = list; item < end; item = next + 1) {
            next = memchr(item, ',', end - item);
            if (next == NULL)
                next = end;
            if (size_fits(item)) {
                if (out != list)
                    *out++ = ',';
                memmove(out, item, next - item);
                out += next - item;
            }
            if (next == end)
                break;
        }
        if (out != list) {
            memmove(out, end, strlen(end) + 1);
        }
    }

    item = find_value(params, "picture-size", &end);
    if (item != NULL && !size_fits(item)) {
        char vga[16];

        len = snprintf(vga, sizeof(vga), "%dx%d", MAX_PICTURE_W, MAX_PICTURE_H);
        if (len <= (size_t) (end - item)) {
            memcpy(item, vga, len);
            memmove(item + len, end, strlen(end) + 1);
        }
    }
}

#define MTK_VIDEO_FORMAT "yuv420i-yyuvyy-3plane"
#define AOSP_VIDEO_FORMAT "yuv420p"

/*
 * Replaces the value of key in params when it is from, in place. to must
 * not be longer than from.
 */
static void rename_value(char *params, const char *key, const char *from, const char *to)
{
    char *end, *v = find_value(params, key, &end);
    size_t len = strlen(to);

    if (v == NULL || (size_t) (end - v) != strlen(from) || strncmp(v, from, end - v) != 0)
        return;
    memcpy(v, to, len);
    memmove(v + len, end, strlen(end) + 1);
}

static char *wrap_get_parameters(struct camera_device *dev)
{
    char *params = mtk_get_parameters(dev);

    if (params != NULL) {
        drop_hdr(params);
        limit_picture_size(params);
        rename_value(params, "video-frame-format", MTK_VIDEO_FORMAT, AOSP_VIDEO_FORMAT);
    }
    return params;
}

static int wrap_set_parameters(struct camera_device *dev, const char *params)
{
    char *end, *v, *mtk_params;
    size_t head, extra = strlen(MTK_VIDEO_FORMAT) - strlen(AOSP_VIDEO_FORMAT);
    int ret;

    mtk_params = malloc(strlen(params) + extra + 1);
    if (mtk_params == NULL)
        return -ENOMEM;
    strcpy(mtk_params, params);

    v = find_value(mtk_params, "video-frame-format", &end);
    if (v != NULL && (size_t) (end - v) == strlen(AOSP_VIDEO_FORMAT) &&
        strncmp(v, AOSP_VIDEO_FORMAT, end - v) == 0) {
        head = v - mtk_params;
        memmove(v + strlen(MTK_VIDEO_FORMAT), end, strlen(end) + 1);
        memcpy(mtk_params + head, MTK_VIDEO_FORMAT, strlen(MTK_VIDEO_FORMAT));
    }

    ret = mtk_set_parameters(dev, mtk_params);
    free(mtk_params);
    return ret;
}

static int wrap_store_meta_data_in_buffers(struct camera_device *dev, int enable)
{
    (void) dev;
    return enable ? -EINVAL : 0;
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
    end_capture();
}

static void wrap_notify_cb(int32_t msg_type, int32_t ext1, int32_t ext2, void *user)
{
    if (msg_type == CAMERA_MSG_ERROR)
        end_capture();
    app_notify_cb(msg_type, ext1, ext2, user);
}

static int wrap_take_picture(struct camera_device *dev)
{
    int ret;

    pthread_mutex_lock(&capture_lock);
    capturing = 1;
    pthread_mutex_unlock(&capture_lock);
    ret = mtk_take_picture(dev);
    if (ret != 0)
        end_capture();
    return ret;
}

static void wrap_stop_preview(struct camera_device *dev)
{
    wait_capture();
    mtk_stop_preview(dev);
}

static void wrap_release(struct camera_device *dev)
{
    wait_capture();
    mtk_release(dev);
}

static void wrap_set_callbacks(struct camera_device *dev,
                               camera_notify_callback notify_cb,
                               camera_data_callback data_cb,
                               camera_data_timestamp_callback data_cb_timestamp,
                               camera_request_memory get_memory, void *user)
{
    app_data_cb = data_cb;
    app_get_memory = get_memory;
    app_notify_cb = notify_cb;
    mtk_set_callbacks(dev, notify_cb != NULL ? wrap_notify_cb : NULL,
                      data_cb != NULL ? wrap_data_cb : NULL,
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
        mtk_take_picture = ops.take_picture;
        ops.take_picture = wrap_take_picture;
        mtk_stop_preview = ops.stop_preview;
        ops.stop_preview = wrap_stop_preview;
        mtk_release = ops.release;
        ops.release = wrap_release;
        mtk_set_parameters = ops.set_parameters;
        ops.set_parameters = wrap_set_parameters;
        ops.store_meta_data_in_buffers = wrap_store_meta_data_in_buffers;
    }
    end_capture();
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
