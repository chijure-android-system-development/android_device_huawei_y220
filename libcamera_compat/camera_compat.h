#ifndef Y220_CAMERA_COMPAT_H
#define Y220_CAMERA_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Calls the takePicture(int) exported only by the stock MTK client. */
int y220_camera_take_picture(void *camera, int msg_type);

#ifdef __cplusplus
}
#endif

#endif
