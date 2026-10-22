LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := libbt_mtk_a2dp
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_SRC_FILES := bt_mtk_a2dp.c
LOCAL_SHARED_LIBRARIES := libcutils libdl
include $(BUILD_SHARED_LIBRARY)
