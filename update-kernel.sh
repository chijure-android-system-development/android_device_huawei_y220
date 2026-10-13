#!/bin/bash
# Copy the zImage and the kernel modules from a built android_kernel_huawei_y220-u05
# tree into this device tree. Modules are stripped of debug info; /system is small.
# Every module in /system/lib/modules must come from the kernel this tree ships:
# the kernel loads mismatched ones (same vermagic, no MODVERSIONS) and can panic.
#
#   KERNEL_DIR=~/y220/kernel-mt6572 ./update-kernel.sh

set -e
HERE=$(cd "$(dirname "$0")" && pwd)
KERNEL_DIR=${KERNEL_DIR:-$HOME/y220/kernel-mt6572}
STRIP=${STRIP:-$HOME/arm-eabi-4.6/bin/arm-eabi-strip}
OUT=$HERE/prebuilt/modules

# Built by the kernel tree and loaded from /system/lib/modules.
MODULES="ccci ccci_plat cfg80211 devapc devinfo mtk_fm_drv mtk_stp_bt mtk_stp_gps
mtk_stp_wmt mtk_wmt_wifi sec vcodec_kernel_driver wlan_mt6582"

ZIMAGE=$KERNEL_DIR/arch/arm/boot/zImage
[ -f "$ZIMAGE" ] || { echo "No zImage in $KERNEL_DIR; build the kernel first." >&2; exit 1; }
[ -x "$STRIP" ] || { echo "No strip at $STRIP; set STRIP." >&2; exit 1; }

cp "$ZIMAGE" "$HERE/kernel"
mkdir -p "$OUT"
for m in $MODULES; do
    src=$(find "$KERNEL_DIR" -name "$m.ko" -not -path '*/out/*' | head -1)
    [ -n "$src" ] || { echo "Module $m.ko not found in $KERNEL_DIR" >&2; exit 1; }
    "$STRIP" --strip-debug -o "$OUT/$m.ko" "$src"
done

python3 - "$HERE/kernel" <<'EOF'
import sys, zlib
d = open(sys.argv[1], 'rb').read()
o = zlib.decompressobj(16 + zlib.MAX_WBITS).decompress(d[d.find(b'\x1f\x8b\x08'):])
i = o.find(b'Linux version')
print(o[i:o.find(b'\n', i)].decode(errors='replace'))
EOF
echo "Modules: $(ls "$OUT" | wc -l) in prebuilt/modules"
