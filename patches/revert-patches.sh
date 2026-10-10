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

revert frameworks/base frameworks_base_egl_bgra.patch
revert hardware/libhardware libhardware_gralloc_bgra.patch
revert hardware/alsa_sound alsa_sound_null_mixer.patch
revert build build_releasetools_ubifs.patch
revert system/core system_core_init_ubi.patch
revert vendor/cyanogen vendor_cyanogen_products.patch

echo "  Eliminando vendor/cyanogen/products/cyanogen_y220.mk"
rm -f "$CM7_ROOT/vendor/cyanogen/products/cyanogen_y220.mk"
