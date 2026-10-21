LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# Object Push for packages/apps/Bluetooth over mtkbt: BlueZ RFCOMM does not
# exist on this kernel, so BluetoothOppMtkClientSession sends through the
# stock libextopp.so (dlopen'ed, it is a vendor blob).
LOCAL_MODULE := libbt_mtk_opp
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_SRC_FILES := bt_mtk_opp.cpp
LOCAL_C_INCLUDES := $(JNI_H_INCLUDE)
LOCAL_SHARED_LIBRARIES := libcutils liblog libdl

include $(BUILD_SHARED_LIBRARY)
