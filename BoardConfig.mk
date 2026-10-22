# Huawei Y220-U05, MediaTek MT6572. Not the Y210 (MSM7x27A).

USE_CAMERA_STUB := true
# The stock libcamera_client/libcameraservice and camera.default.so (from
# vendor) replace the stub at install. The JNI then calls the stock
# takePicture(int). Stock CameraService is 0x68 bytes (stock mediaserver
# allocates that); this tree's header says 0x48.
BOARD_USES_MTK_CAMERA := true
BOARD_MTK_CAMERA_SERVICE_SIZE := 104
# The stock CameraClient registers a new preview heap after takePicture
# without unregistering the old one; without this the preview drifts.
BOARD_SURFACEFLINGER_REREGISTER_BUFFERS := true

TARGET_NO_BOOTLOADER := true
TARGET_NO_RADIOIMAGE := true

TARGET_BOARD_PLATFORM := mt6572
TARGET_BOOTLOADER_BOARD_NAME := y220
TARGET_OTA_ASSERT_DEVICE := y220,HWY220-U,Y220-U05

TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_ARCH_VARIANT := armv7-a-neon
ARCH_ARM_HAVE_TLS_REGISTER := true

BUILD_SREC_GRAMMARS := false

WITH_JIT := true
ENABLE_JSC_JIT := true
JS_ENGINE := v8
HTTP := chrome

# The header cmdline on the stock boot image is empty. The running kernel
# carries its own command line.
BOARD_KERNEL_CMDLINE :=
BOARD_KERNEL_BASE := 0x10000000
BOARD_KERNEL_PAGESIZE := 2048
TARGET_PREBUILT_KERNEL := device/huawei/y220/kernel

# mtkbootimg adds the KERNEL and ROOTFS headers. The OTA then writes that
# image with TWRP's flash_image. See bootimg.mk.
BOARD_CUSTOM_BOOTIMG := true
BOARD_CUSTOM_BOOTIMG_MK := device/huawei/y220/bootimg.mk
BOARD_MTK_BOOTIMG := true

# NAND erase size is 256 KB. Sizes match /proc/mtd on C482B206SP05.
BOARD_FLASH_BLOCK_SIZE := 262144
BOARD_BOOTIMAGE_PARTITION_SIZE := 6291456
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 6291456
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 229638144
BOARD_USERDATAIMAGE_PARTITION_SIZE := 180617216
BOARD_CACHEIMAGE_PARTITION_SIZE := 69206016

# system/data/cache are UBIFS. A yaffs or ext4 system.img from this tree
# must not be written to those partitions.
BOARD_HAS_NO_MISC_PARTITION := true

BOARD_HAVE_BLUETOOTH := true
# Settings enable talks to mtkbt on bt.int.adp, not bluetoothd.
BOARD_HAVE_BLUETOOTH_MTK := true
BOARD_BLUETOOTH_MTK_INCLUDES := device/huawei/y220/libbt_mtk_gap

# libpalwlan_mtk.so needs __xlog_buf_printf from liblog. The body is
# device/huawei/y220/libxlog and is pulled into the shared liblog only.
BOARD_LIBLOG_EXTRA_STATIC := libxlog

# libMali.so needs android::Tracer from libutils. Stock has it; CM7 does not.
BOARD_LIBUTILS_EXTRA_SRC := ../../../../device/huawei/y220/libutils/Tracer.cpp

# Gingerbread audioflinger links libaudio. The MSM7k HAL is not built for
# this platform. ALSA has no card here (CONFIG_SOUND is off). Playback is
# /dev/eac, so the device tree provides libaudio and the ALSA one is skipped.
BOARD_USES_ALSA_AUDIO := true
TARGET_PROVIDES_LIBAUDIO := true

# Combo Wi-Fi. The module on the phone is wlan_mt6582.ko (WEXT).
BOARD_WPA_SUPPLICANT_DRIVER := WEXT
WPA_SUPPLICANT_VERSION := VER_0_6_X
# SIOCSIWPRIV on this driver sets the country code. The Android combo
# scan uses that ioctl, so it never sends SIOCSIWSCAN.
BOARD_WEXT_NO_COMBO_SCAN := true
# IFF_UP on DRIVER START, UI scans stay out of WPA_IDLE, SCAN_RESULTS
# re-reads the driver. Same glue as other MT6572 ports on wlan_mt6582.ko.
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := libwpa_mtk_wext
BOARD_WPA_SUPPLICANT_PRIVATE_INCLUDES := device/huawei/y220/libwpa_mtk_wext
WIFI_DRIVER_MODULE_PATH := /system/lib/modules/wlan_mt6582.ko
WIFI_DRIVER_MODULE_NAME := wlan_mt6582
WIFI_DRIVER_MODULE_ARG := ""

# MTK OMX video decoders (OMX.MTK.VIDEO.DECODER.*) in OMXCodec, and the
# SoftwareRenderer crop of their 32-pixel stride padding.
# See frameworks_base_stagefright_mtk_omx.patch and libmtk_shim.
BOARD_USES_MTK_OMX := true
