LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libxlog
LOCAL_SRC_FILES := xlog.c

include $(BUILD_STATIC_LIBRARY)
