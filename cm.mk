# The first PRODUCT_COPY_FILES entry for a destination wins: this replaces
# vendor/cm's apns-conf.xml, which has no Peru (MCC 716) entries.
PRODUCT_COPY_FILES := \
    device/huawei/y220/apns-conf.xml:system/etc/apns-conf.xml

# Two locales: framework-res and every app stay small on a 219 MiB /system.
PRODUCT_LOCALES := en_US es_US

$(call inherit-product, vendor/cm/config/gsm.mk)

TARGET_BOOTANIMATION_NAME := vertical-320x480

$(call inherit-product, vendor/cm/config/common_mini_phone.mk)
$(call inherit-product, device/huawei/y220/device_y220.mk)
$(call inherit-product, device/huawei/y220/kernel-modules.mk)

PRODUCT_DEFAULT_PROPERTY_OVERRIDES += persist.service.adb.enable=1
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += persist.sys.root_access=3

PRODUCT_RELEASE_NAME := Y220
PRODUCT_DEVICE := y220
PRODUCT_NAME := cm_y220
PRODUCT_BRAND := Huawei
PRODUCT_MANUFACTURER := HUAWEI
PRODUCT_MODEL := HUAWEI Y220-U05
