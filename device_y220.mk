$(call inherit-product, $(SRC_TARGET_DIR)/product/languages_full.mk)
$(call inherit-product-if-exists, vendor/huawei/y220/y220-vendor.mk)

ifeq ($(TARGET_PREBUILT_KERNEL),)
    LOCAL_KERNEL := device/huawei/y220/kernel
else
    LOCAL_KERNEL := $(TARGET_PREBUILT_KERNEL)
endif

PRODUCT_COPY_FILES += \
    $(LOCAL_KERNEL):kernel

PRODUCT_COPY_FILES += \
    device/huawei/y220/init.rc:root/init.rc \
    device/huawei/y220/y220-wipe-system.sh:system/bin/y220-wipe-system.sh \
    device/huawei/y220/init.d/07wmt:system/etc/init.d/07wmt \
    device/huawei/y220/init.local.rc:system/etc/init.local.rc \
    device/huawei/y220/wpa_supplicant.conf:system/etc/wifi/wpa_supplicant.conf \
    device/huawei/y220/wmt-wifi-on.sh:system/etc/wifi/wmt-wifi-on.sh \
    device/huawei/y220/egl.cfg:system/lib/egl/egl.cfg

PRODUCT_COPY_FILES += \
    frameworks/base/data/etc/handheld_core_hardware.xml:system/etc/permissions/handheld_core_hardware.xml \
    frameworks/base/data/etc/android.hardware.camera.autofocus.xml:system/etc/permissions/android.hardware.camera.autofocus.xml \
    frameworks/base/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
    frameworks/base/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
    frameworks/base/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
    frameworks/base/data/etc/android.hardware.touchscreen.multitouch.distinct.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.distinct.xml

PRODUCT_PROPERTY_OVERRIDES += \
    qemu.hw.mainkeys=0

# ro.product.board is y220, so hw_get_module loads lights.y220.so.
# Without it LightsService never writes lcd-backlight and the slider does nothing.
PRODUCT_PACKAGES += \
    lights.y220 \
    libaudio

# Same place as the Y210 tree. This is the AOSP base (Settings, Phone,
# SystemUI, framework jars). Without it the zip only has the CM extras.
$(call inherit-product, build/target/product/full_base.mk)

PRODUCT_NAME := device_y220
PRODUCT_DEVICE := y220
PRODUCT_BRAND := HUAWEI
PRODUCT_MODEL := Y220-U05
PRODUCT_MANUFACTURER := HUAWEI
