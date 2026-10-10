#!/bin/sh
# Pack a Y220 boot image with the MTK header. Does not flash.
# Default payload is kernel #57 plus the stock C482 ramdisk, which is what
# the phone is already running. Pass another zImage as $2 to try a new kernel
# without touching the ramdisk.
set -e
HERE=$(dirname "$0")
HERE=$(cd "$HERE" && pwd)
KERNEL=${2:-"$HERE/kernel"}
RAMDISK="$HERE/prebuilt/ramdisk-stock.img"
OUT=${1:-/tmp/y220-cm7-boot.img}
MTKBOOTIMG=${MTKBOOTIMG:-/home/chijure/twrp-4.4/out/host/linux-x86/bin/mtkbootimg}

"$MTKBOOTIMG" \
    --kernel "$KERNEL" \
    --ramdisk "$RAMDISK" \
    --cmdline "androidboot.hardware=mt6572" \
    --base 0x10000000 \
    --pagesize 2048 \
    --ramdisk_offset 0x01000000 \
    --tags_offset 0x00000100 \
    --mtk 1 \
    -o "$OUT.unpadded"
python3 - "$OUT" << 'PY'
import sys
out = sys.argv[1]
data = open(out + ".unpadded", "rb").read()
limit = 6291456
if len(data) > limit:
    raise SystemExit("boot image %d exceeds %d" % (len(data), limit))
data += b"\xff" * (limit - len(data))
open(out, "wb").write(data)
print("wrote", out, len(data))
PY
rm -f "$OUT.unpadded"
