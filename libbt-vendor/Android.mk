LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libbt-vendor
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := bt_vendor_mtk.c
LOCAL_C_INCLUDES := external/bluetooth/bluedroid/hci/include
LOCAL_SHARED_LIBRARIES := libcutils liblog libdl
include $(BUILD_SHARED_LIBRARY)
