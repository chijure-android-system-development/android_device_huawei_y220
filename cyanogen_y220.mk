# Huawei Ascend Y220-U05 (MT6572). Do not inherit the Y210 Qualcomm tree.
$(call inherit-product, device/huawei/y220/device_y220.mk)
$(call inherit-product, device/huawei/y220/kernel-modules.mk)

$(call inherit-product, vendor/cyanogen/products/common_full_no_themes.mk)
$(call inherit-product, vendor/cyanogen/products/gsm.mk)

PRODUCT_NAME := cyanogen_y220
PRODUCT_BRAND := HUAWEI
PRODUCT_DEVICE := y220
PRODUCT_MODEL := Y220-U05
PRODUCT_MANUFACTURER := HUAWEI

# Stock blobs and the kernel project name key off this, the same way the
# Y210 tree keeps msm7625a. The first writer of ro.build.product wins.
PRODUCT_PROPERTY_OVERRIDES += \
    ro.build.product=huaqin72_we_b2a_gb3
PRODUCT_BUILD_PROP_OVERRIDES += BUILD_PRODUCT=huaqin72_we_b2a_gb3

PRODUCT_RELEASE_NAME := Y220
PRODUCT_VERSION_DEVICE_SPECIFIC :=
-include vendor/cyanogen/products/common_versions.mk

# 320x480 at 160 dpi. Without this zip, bootanimation falls back to the
# "android" text.
PRODUCT_COPY_FILES += \
    vendor/cyanogen/prebuilt/mdpi/media/bootanimation.zip:system/media/bootanimation.zip
