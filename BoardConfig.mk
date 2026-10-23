# Huawei Y220-U05, MediaTek MT6572. Not the Y210 (MSM7x27A).
# CM10.1 (4.2.2) with the 4.2 MTK userspace of the Y330-U01; see
# proprietary-files.txt for which blob comes from where.

-include vendor/huawei/y220/BoardConfigVendor.mk

TARGET_NO_BOOTLOADER := true
TARGET_NO_RADIOIMAGE := true

TARGET_BOARD_PLATFORM := mt6572
TARGET_BOOTLOADER_BOARD_NAME := y220
TARGET_OTA_ASSERT_DEVICE := y220,HWY220-U,Y220-U05

TARGET_ARCH := arm
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_ARCH_VARIANT := armv7-a-neon
# Cortex-A7. GCC 4.6 has no cortex-a7 tuning; the A9 one is the usual choice
# for A7 ports of 4.2 (same ARMv7 ISA, NEON, VFPv4 unused).
TARGET_ARCH_VARIANT_CPU := cortex-a9
TARGET_CPU_SMP := true
ARCH_ARM_HAVE_TLS_REGISTER := true

# 256 MB of RAM (219 MB usable).
TARGET_ARCH_LOWMEM := true

BUILD_SREC_GRAMMARS := false

# Kernel #95, prebuilt from ~/y220/kernel-mt6572 (update-kernel.sh).
# The header cmdline on the stock boot image is empty; the LK passes its
# own command line.
TARGET_PREBUILT_KERNEL := device/huawei/y220/kernel
BOARD_KERNEL_CMDLINE :=
BOARD_KERNEL_BASE := 0x10000000
BOARD_KERNEL_PAGESIZE := 2048

# mtkbootimg adds the MTK KERNEL and ROOTFS headers (bootimg.mk). The OTA
# writes boot with TWRP's flash_image (releasetools).
BOARD_CUSTOM_BOOTIMG := true
BOARD_CUSTOM_BOOTIMG_MK := device/huawei/y220/bootimg.mk
TARGET_RELEASETOOL_OTA_FROM_TARGET_SCRIPT := device/huawei/y220/releasetools/y220_ota_from_target_files

# NAND erase size is 256 KB. Sizes match /proc/mtd on C482B206SP05.
# system, userdata and cache are UBIFS: system.img from this tree is never
# flashed, the OTA zip unpacks files into the mounted volume.
BOARD_FLASH_BLOCK_SIZE := 262144
BOARD_BOOTIMAGE_PARTITION_SIZE := 6291456
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 6291456
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 229638144
BOARD_USERDATAIMAGE_PARTITION_SIZE := 180617216
BOARD_CACHEIMAGE_PARTITION_SIZE := 69206016
BOARD_HAS_NO_MISC_PARTITION := true
TARGET_RECOVERY_FSTAB := device/huawei/y220/recovery.fstab

# CM's init.rc mounts mtd@system/cache/userdata as yaffs2 in "on fs"; on
# this NAND that erases the UBI partitions. rootdir/init.rc drops those.
TARGET_PROVIDES_INIT_RC := true

# Graphics: Mali-400 r3p2 with the stock gralloc.mt6572 (ION). No
# hwcomposer for now: the stock one needs MTK's SurfaceFlinger.
USE_OPENGL_RENDERER := true
BOARD_EGL_CFG := device/huawei/y220/configs/egl.cfg

# Wi-Fi: wlan_mt6582.ko on the WMT combo, cfg80211. The HAL insmods the
# module, then wmt_wifi_loader powers WMT Wi-Fi on and waits for wlan0.
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
WPA_SUPPLICANT_VERSION := VER_0_8_X
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_mt66xx
BOARD_HOSTAPD_DRIVER := NL80211
BOARD_HOSTAPD_PRIVATE_LIB := lib_driver_cmd_mt66xx
WIFI_DRIVER_MODULE_PATH := /system/lib/modules/wlan_mt6582.ko
WIFI_DRIVER_MODULE_NAME := wlan_mt6582
WIFI_DRIVER_MODULE_ARG := ""
WIFI_FIRMWARE_LOADER := wmt_wifi_loader

# Bluetooth: Bluedroid; libbt-vendor opens the combo through the stock
# libbluetoothdrv.so.
BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_MTK := true
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := device/huawei/y220/bluetooth

# Stock camera HAL (camera.mt6572) through CM's camera service.
USE_CAMERA_STUB := false

# RIL: MediaTekRIL (ro.telephony.ril_class) is built into telephony-common.
BOARD_RIL_CLASS := ../../../device/huawei/y220/ril/telephony/java
