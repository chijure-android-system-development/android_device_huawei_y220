LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# Device-owned mediaserver entry point. The stock MTK CameraService object is
# larger than the CM7 header, so its allocation belongs with this device ABI.
LOCAL_SRC_FILES := main_mediaserver.cpp

LOCAL_SHARED_LIBRARIES := \
    libaudioflinger \
    libcameraservice \
    libmediaplayerservice \
    libutils \
    libbinder

LOCAL_C_INCLUDES := \
    frameworks/base/services/audioflinger \
    frameworks/base/services/camera/libcameraservice \
    frameworks/base/media/libmediaplayerservice

LOCAL_CFLAGS := -DMTK_CAMERA_SERVICE_SIZE=$(BOARD_MTK_CAMERA_SERVICE_SIZE)
LOCAL_MODULE := mediaserver
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
