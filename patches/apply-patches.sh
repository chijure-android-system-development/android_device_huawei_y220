#!/bin/bash
# Aplica los parches del Y220 sobre un árbol CM7 limpio.
# Ejecutar desde la raíz del árbol de CM7.
#
#   cd /path/to/cm7
#   bash device/huawei/y220/patches/apply-patches.sh
#
# En este árbol ya están aplicados. Volver a correrlo falla el --check.

set -e

PATCHES_DIR="$(cd "$(dirname "$0")" && pwd)"
CM7_ROOT="$(pwd)"

apply() {
    local project="$1"
    local patch="$2"
    echo "  Aplicando $patch en $project..."
    (cd "$CM7_ROOT/$project" && git apply --check "$PATCHES_DIR/$patch")
    (cd "$CM7_ROOT/$project" && git apply "$PATCHES_DIR/$patch")
}

copy_new() {
    local src="$1"
    local dst="$2"
    if [ -e "$CM7_ROOT/$dst" ]; then
        echo "  Ya existe $dst"
        exit 1
    fi
    echo "  Copiando $dst"
    mkdir -p "$(dirname "$CM7_ROOT/$dst")"
    cp "$PATCHES_DIR/$src" "$CM7_ROOT/$dst"
}

# vendor/cyanogen — lunch importa vendor/cyanogen/products/cyanogen_$CM_BUILD.mk
apply vendor/cyanogen vendor_cyanogen_products.patch
copy_new cyanogen_y220.mk vendor/cyanogen/products/cyanogen_y220.mk

# build — ubifs en el OTA, boot MTK y flash_image desde TWRP.
# No formatea el volumen. El system.img del árbol sigue siendo yaffs.
apply build build_releasetools_ubifs.patch

# system/core — init entiende "mount ubifs ubi@system /system".
apply system/core system_core_init_ubi.patch

# hardware/alsa_sound — no recorrer un mixer que no se pudo abrir.
apply hardware/alsa_sound alsa_sound_null_mixer.patch

# gralloc.default — mtkfb 32 bpp trae azul en el bit 0.
apply hardware/libhardware libhardware_gralloc_bgra.patch

# PixelFlinger — config EGL con NATIVE_VISUAL_ID BGRA_8888.
apply frameworks/base frameworks_base_egl_bgra.patch

# libutils — android::Tracer, que libMali.so resuelve en libutils.
apply frameworks/base frameworks_base_libutils_tracer.patch

# liblog — el .so compartido absorbe la estática del device tree (libxlog).
apply system/core system_core_liblog_extra.patch

# Wi-Fi HAL — espera a wlan0 cuando existe /dev/wmtWifi.
apply hardware/libhardware_legacy libhardware_legacy_wifi_wmt.patch

# Settings scan: DORMANT sin asociación no debe bajar wlan0.
apply frameworks/base frameworks_base_wifi_dormant.patch

# Settings BT: POWERON por el socket de mtkbt. La trama está en libbt_mtk_gap.
apply frameworks/base frameworks_base_bt_mtk_gap.patch

# wpa_supplicant: llamadas a libwpa_mtk_wext. La política está en el device tree.
apply external/wpa_supplicant_6 external_wpa_supplicant_6_scan_idle.patch

# PhoneFactory: ro.telephony.ril_class=mediatek carga MediaTekRIL del device tree.
apply frameworks/base frameworks_base_phonefactory_mediatek.patch

# init: el proceso phone (radio) escribe net.ccmni* (DNS y gateway de datos).
apply system/core system_core_property_ccmni.patch

# Cámara MTK de stock (camera.default.so + libcam.*, libcamera_client y
# libcameraservice de stock). Funciones de ICS que piden esos blobs:
# String8::format/formatV, Thread::join, PropertyMap.
apply frameworks/base frameworks_base_libutils_ics_compat.patch

# libjpeg: jpeg_compress_struct con los dos arrays de MTK al final (432 bytes)
# y jpeg_mem_dest, que usan libJpgEncPipe, camshot y camadapter.
apply external/jpeg external_jpeg_mtk_compat.patch

# libcutils: pmem_* (libdpframework) y dump_backtrace_to_file (libaed).
apply system/core system_core_libcutils_mtk_compat.patch

# JNI llama al takePicture(int) del cliente de stock y mediaserver reserva el
# tamaño del CameraService de stock (BOARD_USES_MTK_CAMERA, BOARD_MTK_CAMERA_SERVICE_SIZE).
apply frameworks/base frameworks_base_camera_mtk_client.patch

# LayerBuffer: la preview llega en YV12 por registerBuffers/postBuffer;
# convertirla a RGB565 antes de subir la textura.
apply frameworks/base frameworks_base_surfaceflinger_yv12.patch

# Phone: si accept() del socket SCO falla, reintentar a los 30 s. Sin SCO de
# Bluez en el kernel fallaba al instante y com.android.phone giraba al 43%.
apply packages/apps/Phone packages_apps_Phone_sco_retry.patch

# CameraSource: el HAL de MTK manda video en "yuv420i-yyuvyy-3plane" (I420).
# Sin esto CHECK aborta mediaserver al grabar video.
apply frameworks/base frameworks_base_stagefright_mtk_i420.patch

# Bluetooth sin red Bluez: startPan/stopPan fallan en netd. La excepción
# tumbaba system_server al encender BT.
apply frameworks/base frameworks_base_bt_pan_no_crash.patch

# MAP: si no se pudo crear el socket RFCOMM, el hilo de accept termina en vez
# de NullPointerException.
apply packages/apps/Bluetooth packages_apps_Bluetooth_map_no_socket.patch
