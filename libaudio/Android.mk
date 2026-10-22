LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libaudio
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := AudioHardware.cpp
LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libmedia libhardware_legacy
# A2dpAudioInterface (libaudiointerface) calls the liba2dp API. mtkbt takes
# SBC from the stock libmtka2dp, which exports the same functions; the BlueZ
# liba2dp would talk to a bluetoothd that does not run here.
LOCAL_LDFLAGS := vendor/huawei/y220/proprietary/lib/libmtka2dp.so \
    -Wl,-rpath-link,vendor/huawei/y220/proprietary/lib
LOCAL_STATIC_LIBRARIES := libaudiointerface
LOCAL_C_INCLUDES := \
    hardware/libhardware_legacy/include \
    frameworks/base/include

include $(BUILD_SHARED_LIBRARY)

# audioflinger links libaudiopolicy whenever BOARD_USES_GENERIC_AUDIO is off.
# hardware/alsa_sound skips it together with its libaudio when the target
# provides libaudio, so build the same ALSA policy manager here.
include $(CLEAR_VARS)

LOCAL_MODULE := libaudiopolicy
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := ../../../../hardware/alsa_sound/AudioPolicyManagerALSA.cpp
LOCAL_CFLAGS := -D_POSIX_SOURCE
ifeq ($(BOARD_HAVE_BLUETOOTH),true)
    LOCAL_CFLAGS += -DWITH_A2DP
endif
LOCAL_C_INCLUDES := hardware/alsa_sound
LOCAL_WHOLE_STATIC_LIBRARIES := libaudiopolicybase
LOCAL_SHARED_LIBRARIES := libcutils libutils libmedia

include $(BUILD_SHARED_LIBRARY)
