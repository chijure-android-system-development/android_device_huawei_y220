#!/bin/bash
# Quita los parches del Y220. Ejecutar desde la raíz del árbol de CM7.
#
#   cd /path/to/cm7
#   bash device/huawei/y220/patches/revert-patches.sh

set -e

PATCHES_DIR="$(cd "$(dirname "$0")" && pwd)"
CM7_ROOT="$(pwd)"

revert() {
    local project="$1"
    local patch="$2"
    echo "  Revirtiendo $patch en $project..."
    (cd "$CM7_ROOT/$project" && git apply --reverse "$PATCHES_DIR/$patch")
}

revert external/wpa_supplicant_8 external_wpa_supplicant_8_nl80211_eapol.patch
revert system/netd system_netd_logwrapper_waitpid.patch
revert system/netd system_netd_custom_softap.patch
revert packages/apps/Settings packages_apps_Settings_rndis_on_demand.patch
revert frameworks/base frameworks_base_stagefright_mtk_omx.patch
revert frameworks/base frameworks_base_servicemanager_bluetooth.patch
revert packages/apps/Bluetooth packages_apps_Bluetooth_opp_mtk.patch
revert packages/apps/Bluetooth packages_apps_Bluetooth_map_no_socket.patch
revert frameworks/base frameworks_base_bt_pan_no_crash.patch
revert frameworks/base frameworks_base_stagefright_mtk_i420.patch
revert packages/apps/Phone packages_apps_Phone_sco_retry.patch
revert frameworks/base frameworks_base_surfaceflinger_reregister.patch
revert frameworks/base frameworks_base_surfaceflinger_yv12.patch
revert frameworks/base frameworks_base_camera_mtk_client.patch
revert system/core system_core_libcutils_mtk_compat.patch
revert external/jpeg external_jpeg_mtk_compat.patch
revert frameworks/base frameworks_base_libutils_ics_compat.patch
revert system/core system_core_property_ccmni.patch
revert frameworks/base frameworks_base_phonefactory_mediatek.patch
revert external/wpa_supplicant_6 external_wpa_supplicant_6_mtk_wext.patch
revert frameworks/base frameworks_base_bt_mtk.patch
revert frameworks/base frameworks_base_wifi_dormant.patch
revert system/core system_core_liblog_extra.patch
revert frameworks/base frameworks_base_libutils_tracer.patch
revert frameworks/base frameworks_base_egl_bgra.patch
revert hardware/libhardware libhardware_gralloc_bgra.patch
revert hardware/alsa_sound alsa_sound_null_mixer.patch
revert build build_releasetools_ubifs.patch
revert system/core system_core_init_charging.patch
revert system/core system_core_init_ubi.patch
revert vendor/cyanogen vendor_cyanogen_products.patch

echo "  Eliminando vendor/cyanogen/products/cyanogen_y220.mk"
rm -f "$CM7_ROOT/vendor/cyanogen/products/cyanogen_y220.mk"
