LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := libmtk_shim
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_SRC_FILES := bionic_shim.c media_shim.cpp
LOCAL_SHARED_LIBRARIES := libbinder libutils
# pthread_internal_t, for __pthread_gettid.
LOCAL_C_INCLUDES := bionic/libc/bionic
include $(BUILD_SHARED_LIBRARY)
