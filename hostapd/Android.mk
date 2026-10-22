# hostapd 0.8 (nl80211) for the Wi-Fi hotspot. The wlan_mt6582 driver makes
# its AP interface (ap0) cfg80211-only, so the GB hostapd 0.6 cannot drive it.
# external/wpa_supplicant_8 only builds its subdirs for VER_0_8_X, and this
# board keeps wpa_supplicant 0.6, so hostapd is included from here.
ifeq ($(BOARD_HOSTAPD_DRIVER),NL80211)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libhostapd_mtk
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := hostapd_mtk.c
LOCAL_C_INCLUDES := external/openssl/include
include $(BUILD_STATIC_LIBRARY)

# The hostapd makefile sets global variables (CONFIG_*, NEED_*, OBJS...) that
# wpa_supplicant_6 reads too; with CONFIG_DRIVER_NL80211 left set it builds
# its nl80211 driver and fails. Save them and put them back afterwards.
hostapd_leaks := AESOBJS CHAP HOBJS INCLUDES L_CFLAGS LDFLAGS LIBS LIBS_c \
    LIBS_h MS_FUNCS OBJS OBJS_c OBJS_d OBJS_p PN531_PATH SHA1OBJS TLS_FUNCS \
    WPA_BUILD_HOSTAPD
hostapd_vars := $(sort $(filter CONFIG_% NEED_%,$(.VARIABLES)) $(hostapd_leaks))
$(foreach v,$(hostapd_vars),$(eval hostapd_save.$(v) := $$(value $(v))))

BOARD_HOSTAPD_PRIVATE_LIB := libhostapd_mtk
include external/wpa_supplicant_8/hostapd/Android.mk

$(foreach v,$(sort $(filter CONFIG_% NEED_%,$(.VARIABLES)) $(hostapd_leaks)), \
    $(eval $(v) := $$(hostapd_save.$(v))))

endif
