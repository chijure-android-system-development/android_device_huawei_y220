LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := dltest
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := dltest.c
LOCAL_SHARED_LIBRARIES := libdl
include $(BUILD_EXECUTABLE)
