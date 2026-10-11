LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# MT6620/mtk WEXT glue for Gingerbread. SIOCSIWPRIV in gl_wext.c is the
# country code, not the Broadcom private commands. Another MT6572 port
# with wlan_mt6582.ko can point BOARD_WPA_SUPPLICANT_PRIVATE_LIB here.
LOCAL_MODULE := libwpa_mtk_wext
LOCAL_SRC_FILES := wpa_mtk_wext.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)
# Y220 NVRAM has no country. Channels 1-13. Change the two letters per port.
LOCAL_CFLAGS := -DMTK_WEXT_COUNTRY=\"EU\"

include $(BUILD_STATIC_LIBRARY)
