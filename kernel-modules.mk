# Kernel modules built with the kernel in device/huawei/y220/kernel
# (android_kernel_huawei_y220-u05). Refresh both with update-kernel.sh.
# They are GPL (or Dual BSD/GPL) and built from that public tree, so they
# live here and not in vendor/. p2p.ko is a stock binary and stays in vendor.
PRODUCT_COPY_FILES += \
    device/huawei/y220/prebuilt/modules/ccci.ko:system/lib/modules/ccci.ko \
    device/huawei/y220/prebuilt/modules/ccci_plat.ko:system/lib/modules/ccci_plat.ko \
    device/huawei/y220/prebuilt/modules/cfg80211.ko:system/lib/modules/cfg80211.ko \
    device/huawei/y220/prebuilt/modules/devapc.ko:system/lib/modules/devapc.ko \
    device/huawei/y220/prebuilt/modules/devinfo.ko:system/lib/modules/devinfo.ko \
    device/huawei/y220/prebuilt/modules/mtk_fm_drv.ko:system/lib/modules/mtk_fm_drv.ko \
    device/huawei/y220/prebuilt/modules/mtk_stp_bt.ko:system/lib/modules/mtk_stp_bt.ko \
    device/huawei/y220/prebuilt/modules/mtk_stp_gps.ko:system/lib/modules/mtk_stp_gps.ko \
    device/huawei/y220/prebuilt/modules/mtk_stp_wmt.ko:system/lib/modules/mtk_stp_wmt.ko \
    device/huawei/y220/prebuilt/modules/mtk_wmt_wifi.ko:system/lib/modules/mtk_wmt_wifi.ko \
    device/huawei/y220/prebuilt/modules/sec.ko:system/lib/modules/sec.ko \
    device/huawei/y220/prebuilt/modules/vcodec_kernel_driver.ko:system/lib/modules/vcodec_kernel_driver.ko \
    device/huawei/y220/prebuilt/modules/wlan_mt6582.ko:system/lib/modules/wlan_mt6582.ko
