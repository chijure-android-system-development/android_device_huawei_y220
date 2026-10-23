$(call inherit-product-if-exists, vendor/huawei/y220/y220-vendor.mk)

DEVICE_PACKAGE_OVERLAYS += device/huawei/y220/overlay

PRODUCT_COPY_FILES += \
    device/huawei/y220/kernel:kernel

# Boot
PRODUCT_COPY_FILES += \
    device/huawei/y220/rootdir/init.rc:root/init.rc \
    device/huawei/y220/rootdir/init.mt6572.rc:root/init.mt6572.rc \
    device/huawei/y220/rootdir/init.mt6572.usb.rc:root/init.mt6572.usb.rc \
    device/huawei/y220/rootdir/init.modem.rc:root/init.modem.rc \
    device/huawei/y220/rootdir/init.connectivity.rc:root/init.connectivity.rc \
    device/huawei/y220/rootdir/ueventd.mt6572.rc:root/ueventd.mt6572.rc \
    device/huawei/y220/rootdir/init.mtk-nodes.sh:system/etc/init.mtk-nodes.sh

PRODUCT_PACKAGES += \
    ubiattach_y220

# Symbols the MTK blobs expect from MTK's liblog, libmedia, libui and
# libcamera_client (LD_PRELOAD from init.mt6572.rc).
PRODUCT_PACKAGES += \
    libmtk_symbols

# Graphics, audio, lights
PRODUCT_PACKAGES += \
    audio.primary.mt6572 \
    audio_policy.default \
    audio.a2dp.default \
    audio.usb.default \
    audio.r_submix.default \
    lights.y220

# Wi-Fi
PRODUCT_COPY_FILES += \
    device/huawei/y220/configs/wpa_supplicant.conf:system/etc/wifi/wpa_supplicant.conf \
    device/huawei/y220/wmt-wifi-on.sh:system/etc/wifi/wmt-wifi-on.sh

# Bluetooth
PRODUCT_PACKAGES += \
    libbt-vendor

# Telephony: the stock rild comes from vendor (proprietary-files.txt).
PRODUCT_PACKAGES += \
    Mms

# Configs
PRODUCT_COPY_FILES += \
    device/huawei/y220/configs/audio_policy.conf:system/etc/audio_policy.conf \
    device/huawei/y220/configs/media_codecs.xml:system/etc/media_codecs.xml \
    device/huawei/y220/configs/vold.fstab:system/etc/vold.fstab \
    device/huawei/y220/configs/gps.conf:system/etc/gps.conf \
    device/huawei/y220/keylayout/ACCDET.kl:system/usr/keylayout/ACCDET.kl \
    device/huawei/y220/keylayout/mtk-kpd.kl:system/usr/keylayout/mtk-kpd.kl \
    device/huawei/y220/keylayout/mtk-tpd-kpd.kl:system/usr/keylayout/mtk-tpd-kpd.kl \
    device/huawei/y220/keylayout/mtk-tpd.kl:system/usr/keylayout/mtk-tpd.kl \
    device/huawei/y220/idc/mtk-tpd.idc:system/usr/idc/mtk-tpd.idc

# Features: no GPS, autofocus or compass (handheld_core_hardware without
# the compass).
PRODUCT_COPY_FILES += \
    device/huawei/y220/permissions/handheld_core_hardware.xml:system/etc/permissions/handheld_core_hardware.xml \
    frameworks/native/data/etc/android.hardware.camera.xml:system/etc/permissions/android.hardware.camera.xml \
    frameworks/native/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
    frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.multitouch.distinct.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.distinct.xml \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml

PRODUCT_PROPERTY_OVERRIDES += \
    ro.sf.lcd_density=160 \
    wifi.interface=wlan0 \
    ro.opengles.version=131072 \
    rild.libpath=/system/lib/mtk-ril.so \
    ro.telephony.ril_class=MediaTekRIL \
    mobiledata.interfaces=ccmni0,rmnet0,eth0,gprs,ppp0 \
    persist.mtk.wcn.combo.chipid=0x6572 \
    ro.mediatek.platform=MT6572 \
    ro.mediatek.version.release=ALPS.JB3.MP.V1.37 \
    persist.gemini.sim_num=1 \
    ro.mediatek.gemini_support=false \
    dalvik.vm.heapstartsize=5m \
    dalvik.vm.heapgrowthlimit=32m \
    dalvik.vm.heapsize=64m

# HVGA (320x480, mdpi) phone.
PRODUCT_AAPT_CONFIG := normal mdpi
PRODUCT_AAPT_PREF_CONFIG := mdpi

# Same base as the Y210: telephony.mk would add CM's rild, which would
# install over the stock MTK one.
$(call inherit-product, build/target/product/generic_no_telephony.mk)

PRODUCT_PACKAGES += \
    Phone \
    Settings \
    LatinIME \
    Calendar \
    DeskClock \
    Email \
    Exchange \
    Camera \
    Bluetooth \
    SoundRecorder \
    SystemUI \
    Trebuchet
