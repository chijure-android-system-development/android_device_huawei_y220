PRODUCT_COPY_FILES += \
    device/huawei/y220/ramdisk/recovery/twrp.fstab:recovery/root/etc/twrp.fstab \
    device/huawei/y220/ramdisk/init.recovery.mt6572.rc:root/init.recovery.mt6572.rc \
    device/huawei/y220/ramdisk/sbin/memguard.sh:recovery/root/sbin/memguard.sh

PRODUCT_PACKAGES += \
    adbd \
    ubiattach

PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    ro.adb.secure=0 \
    persist.service.adb.enable=1 \
    service.adb.root=1
