$(call inherit-product, $(SRC_TARGET_DIR)/product/languages_full.mk)
$(call inherit-product-if-exists, vendor/huawei/y220/y220-vendor.mk)

# MTK preloader is not a fastboot bootloader. Drop that reboot choice.
DEVICE_PACKAGE_OVERLAYS += device/huawei/y220/overlay

# MediaTekRIL goes into framework.jar. Relative to frameworks/base.
FRAMEWORKS_BASE_SUBDIRS += ../../device/huawei/y220/ril/telephony/java

ifeq ($(TARGET_PREBUILT_KERNEL),)
    LOCAL_KERNEL := device/huawei/y220/kernel
else
    LOCAL_KERNEL := $(TARGET_PREBUILT_KERNEL)
endif

PRODUCT_COPY_FILES += \
    $(LOCAL_KERNEL):kernel

PRODUCT_COPY_FILES += \
    device/huawei/y220/init.rc:root/init.rc \
    device/huawei/y220/ueventd.mt6572.rc:root/ueventd.mt6572.rc \
    device/huawei/y220/y220-wipe-system.sh:system/bin/y220-wipe-system.sh \
    device/huawei/y220/init.d/06modem:system/etc/init.d/06modem \
    device/huawei/y220/init.d/07wmt:system/etc/init.d/07wmt \
    device/huawei/y220/init.d/08zram:system/etc/init.d/08zram \
    device/huawei/y220/init.d/09thermal:system/etc/init.d/09thermal \
    device/huawei/y220/init.d/10vcodec:system/etc/init.d/10vcodec \
    device/huawei/y220/init.local.rc:system/etc/init.local.rc \
    device/huawei/y220/wpa_supplicant.conf:system/etc/wifi/wpa_supplicant.conf \
    device/huawei/y220/wmt-wifi-on.sh:system/etc/wifi/wmt-wifi-on.sh \
    device/huawei/y220/apns-conf.xml:system/etc/apns-conf.xml \
    device/huawei/y220/gps.conf:system/etc/gps.conf \
    device/huawei/y220/FMRadio/com.mediatek.fmradio.xml:system/etc/permissions/com.mediatek.fmradio.xml \
    device/huawei/y220/egl.cfg:system/lib/egl/egl.cfg \
    device/huawei/y220/keylayout/ACCDET.kl:system/usr/keylayout/ACCDET.kl

PRODUCT_COPY_FILES += \
    device/huawei/y220/permissions/handheld_core_hardware.xml:system/etc/permissions/handheld_core_hardware.xml \
    frameworks/base/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
    frameworks/base/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
    frameworks/base/data/etc/android.hardware.touchscreen.multitouch.distinct.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.distinct.xml

PRODUCT_PROPERTY_OVERRIDES += \
    qemu.hw.mainkeys=0

# ro.product.board is y220, so hw_get_module loads lights.y220.so.
# Without it LightsService never writes lcd-backlight and the slider does nothing.
PRODUCT_PACKAGES += \
    lights.y220 \
    libaudio \
    libaudiopolicy \
    mediaserver \
    libbt_mtk_gap \
    libbt_mtk_opp \
    libbt_mtk_a2dp \
    libmtk_shim \
    hostapd \
    FMRadio

# Same place as the Y210 tree. This is the AOSP base (Settings, Phone,
# SystemUI, framework jars). Without it the zip only has the CM extras.
$(call inherit-product, build/target/product/full_base.mk)

PRODUCT_NAME := device_y220
PRODUCT_DEVICE := y220
PRODUCT_BRAND := HUAWEI
PRODUCT_MODEL := Y220-U05
PRODUCT_MANUFACTURER := HUAWEI
