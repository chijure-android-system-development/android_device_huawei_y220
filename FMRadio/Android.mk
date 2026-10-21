LOCAL_PATH := $(call my-dir)

# Stock MediaTek FMRadio, deodexed and rebuilt by build-fmradio.sh (see README).
include $(CLEAR_VARS)
LOCAL_MODULE := FMRadio
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_SRC_FILES := FMRadio.apk
# ACCESS_MTK_MMHW (gid media for /dev/fm) is signatureOrSystem.
LOCAL_CERTIFICATE := platform
include $(BUILD_PREBUILT)
