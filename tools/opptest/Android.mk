LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := opptest
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := opptest.c
LOCAL_SHARED_LIBRARIES := libcutils libdl
include $(BUILD_EXECUTABLE)
