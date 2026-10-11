LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# Gingerbread Settings talks to mtkbt on bt.int.adp. The GAP ids match the
# non-WISE Android layout used by the MT6620 daemon. Another MT6572 port
# can point BOARD_HAVE_BLUETOOTH_MTK at this module.
LOCAL_MODULE := libbt_mtk_gap
LOCAL_SRC_FILES := bt_mtk_gap.c
LOCAL_C_INCLUDES := $(LOCAL_PATH) system/core/include

include $(BUILD_STATIC_LIBRARY)
