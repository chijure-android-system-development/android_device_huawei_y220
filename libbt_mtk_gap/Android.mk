LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# Gingerbread Settings talks to mtkbt on bt.int.adp. The GAP ids match the
# non-WISE Android layout used by the MT6620 daemon. Another MT6572 port
# can point BOARD_HAVE_BLUETOOTH_MTK at this module. Keep the protocol in a
# device-owned shared library instead of embedding it in libandroid_runtime.
LOCAL_MODULE := libbt_mtk_gap
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_SRC_FILES := bt_mtk_gap.c
LOCAL_C_INCLUDES := $(LOCAL_PATH) system/core/include
LOCAL_SHARED_LIBRARIES := libcutils liblog

include $(BUILD_SHARED_LIBRARY)
