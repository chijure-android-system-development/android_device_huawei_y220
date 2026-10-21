#!/usr/bin/env bash
# blobcheck.sh — busca blobs rotos en el /system del Y220 conectado por ADB.
#
# Uso:
#   bash device/huawei/y220/tools/blobcheck/blobcheck.sh [--stock <system.ubifs.win>]
#
# 1. Copia /system/lib, bin y xbin del teléfono y corre symcheck.py:
#    bibliotecas NEEDED que faltan y símbolos sin definir.
# 2. Sube dltest y hace dlopen de cada .so de /system/lib (un proceso
#    por biblioteca). Encuentra lo que el linker de GB rechaza al cargar.
# 3. Con --stock, lista archivos y servicios de init del backup TWRP stock
#    que no están en CM7.
#
# Las líneas que coinciden con known.txt salen como conocidas; el resto es
# NUEVO y el script termina con 1. dltest se compila con
#   mmm device/huawei/y220/tools/blobcheck
# y se busca en $DLTEST, $ANDROID_PRODUCT_OUT/system/bin o el contenedor
# cm7-builder. dlopen corre los constructores de cada biblioteca en un
# proceso hijo que sale enseguida.

HERE=$(cd "$(dirname "$0")" && pwd)
KNOWN="$HERE/known.txt"
STOCK=""
[[ "$1" == "--stock" ]] && STOCK="$2"
WORK=$(mktemp -d /tmp/blobcheck.XXXXXX)
NEW=0

RED='\033[0;31m'; GRN='\033[0;32m'; YLW='\033[1;33m'; BLD='\033[1m'; RST='\033[0m'

# Separa las líneas en conocidas y nuevas según known.txt.
classify() {
    local out=$1 known new
    known=$(grep -v '^#' "$KNOWN" | grep -v '^$' | grep -F -f - "$out")
    new=$(grep -v '^#' "$KNOWN" | grep -v '^$' | grep -v -F -f - "$out")
    [ -n "$known" ] && echo -e "${YLW}conocidos: $(echo "$known" | wc -l)${RST}"
    if [ -n "$new" ]; then
        echo -e "${RED}NUEVOS:${RST}"; echo "$new"
        NEW=1
    else
        echo -e "${GRN}sin problemas nuevos${RST}"
    fi
}

adb root >/dev/null 2>&1; sleep 1
adb get-state >/dev/null 2>&1 || { echo "sin dispositivo ADB"; exit 2; }

echo -e "${BLD}== NEEDED y símbolos (symcheck.py)${RST}"
mkdir -p "$WORK/system"
for d in lib bin xbin; do adb pull /system/$d "$WORK/system/$d" >/dev/null 2>&1; done
python3 "$HERE/symcheck.py" "$WORK/system" | sort > "$WORK/sym.txt"
classify "$WORK/sym.txt"

echo -e "${BLD}== dlopen en el teléfono (dltest)${RST}"
BIN=${DLTEST:-}
[ -z "$BIN" ] && [ -n "$ANDROID_PRODUCT_OUT" ] && BIN="$ANDROID_PRODUCT_OUT/system/bin/dltest"
if [ ! -f "$BIN" ] && docker exec cm7-builder true 2>/dev/null; then
    docker cp cm7-builder:/home/builder/cm7/out/target/product/y220/system/bin/dltest \
        "$WORK/dltest" >/dev/null 2>&1 && BIN="$WORK/dltest"
fi
if [ ! -f "$BIN" ]; then
    echo "dltest no encontrado; compílalo con: mmm device/huawei/y220/tools/blobcheck"
else
    adb push "$BIN" /data/local/tmp/dltest >/dev/null
    adb shell 'chmod 755 /data/local/tmp/dltest; cd /system/lib;
        /data/local/tmp/dltest *.so hw/*.so egl/*.so' | tr -d '\r' \
        | grep -v ': OK$' | sort > "$WORK/dl.txt"
    classify "$WORK/dl.txt"
fi

if [ -n "$STOCK" ]; then
    echo -e "${BLD}== archivos stock que no están en CM7${RST}"
    tar tf "$STOCK" 2>/dev/null | sed 's#^/*##; s#/$##' | grep -v '^$' \
        | grep -v '\.odex$' | sort -u > "$WORK/stock.txt"
    adb shell 'busybox find /system' | tr -d '\r' | sed 's#^/system/##' \
        | sort -u > "$WORK/cm7.txt"
    comm -23 "$WORK/stock.txt" "$WORK/cm7.txt" \
        | grep -E '^(bin|xbin|lib|etc|usr/keylayout|usr/keychars)/' | column -c 160
fi

echo "salida en $WORK"
exit $NEW
