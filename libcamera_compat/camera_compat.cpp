#define LOG_TAG "Y220CameraCompat"

#include <dlfcn.h>
#include <errno.h>
#include <utils/Log.h>

#include "camera_compat.h"

typedef int (*take_picture_t)(void *, int);

extern "C" int y220_camera_take_picture(void *camera, int msg_type)
{
    static take_picture_t take_picture;
    static bool resolved;

    if (!resolved) {
        void *handle = dlopen("libcamera_client.so", RTLD_NOW);
        if (handle != NULL) {
            take_picture = reinterpret_cast<take_picture_t>(
                    dlsym(handle, "_ZN7android6Camera11takePictureEi"));
        }
        if (take_picture == NULL)
            LOGE("stock Camera::takePicture(int) is unavailable: %s", dlerror());
        resolved = true;
    }

    return take_picture != NULL ? take_picture(camera, msg_type) : -ENOSYS;
}
