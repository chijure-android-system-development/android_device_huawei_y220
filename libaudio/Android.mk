LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libaudio
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := AudioHardware.cpp
LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libmedia libhardware_legacy liba2dp
LOCAL_STATIC_LIBRARIES := libaudiointerface
LOCAL_C_INCLUDES := \
    hardware/libhardware_legacy/include \
    frameworks/base/include

include $(BUILD_SHARED_LIBRARY)
