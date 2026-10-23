LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := audio_hw_mtk.c
LOCAL_MODULE := audio.primary.mt6572
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES := system/media/audio_utils/include
LOCAL_SHARED_LIBRARIES := liblog libdl libaudioutils
include $(BUILD_SHARED_LIBRARY)
