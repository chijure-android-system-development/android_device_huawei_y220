# Symbols that the stock MTK blobs (Y330-U01 4.2.2) expect from MTK's own
# liblog, libcutils, libjpeg, libmedia, libui and libcamera_client. init.mt6572.rc
# puts this library in LD_PRELOAD; the 4.2 linker searches preloads for
# every library's undefined symbols, so CM's libraries stay untouched.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
    xlog.c \
    pmem.c \
    mtk_audio.cpp \
    mtk_ui.cpp \
    MtkCameraParameters.cpp \
    jpeg_mem_dest.c
LOCAL_C_INCLUDES := external/jpeg
LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libhardware
LOCAL_MODULE := libmtk_symbols
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
