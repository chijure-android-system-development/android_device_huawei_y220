#!/usr/bin/env bash
# test_y220.sh — smoke-test y caza de bugs del port CM7 para el Huawei Y220
# (MT6572 + combo WMT) vía ADB.
#
# Uso (desde raíz del árbol CM7 o en cualquier directorio):
#   bash device/huawei/y220/tools/test_y220.sh [--fast] [--section <nombre>]
#        [--zip <cm-7-*.zip>]
#
# Opciones:
#   --fast        Omite tests que requieren espera
#   --section X   Ejecuta solo esa sección (zip|boot|bugs|display|audio|wifi|
#                 bt|fm|sensors|ril|storage|power)
#   --zip F       Antes de flashear: prueba que cada apk/jar del zip sea un zip
#                 válido. Con --section zip no hace falta el teléfono.
#
# Requiere: adb en PATH, un dispositivo conectado (o ANDROID_SERIAL=Y220-U05).

RED='\033[0;31m'; GRN='\033[0;32m'; YLW='\033[1;33m'
CYN='\033[0;36m'; BLD='\033[1m'; RST='\033[0m'

PASS=0; FAIL=0; SKIP=0; MANUAL=0
FAST=false; FILTER=""; ZIP=""
# Stub de 3449 bytes de vendor/cyanogen/proprietary; CM7 lo trae así.
KNOWN_BAD_APKS="RomManager.apk"
LOG=/tmp/test_y220_$(date +%Y%m%d_%H%M%S).log
echo "test_y220.sh  $(date)" > "$LOG"

_arg=""
for a in "$@"; do
    if [[ "$_arg" == "--section" ]]; then FILTER="$a"; _arg=""; continue; fi
    if [[ "$_arg" == "--zip" ]]; then ZIP="$a"; _arg=""; continue; fi
    case $a in --fast) FAST=true ;; --section) _arg="--section" ;; --zip) _arg="--zip" ;; esac
done

# ── primitivas ────────────────────────────────────────────────────────────────

adb_sh() { adb shell "$@" 2>/dev/null | tr -d '\r'; }

pass()   { echo -e "${GRN}  ✓ PASS${RST}  $1"; PASS=$((PASS+1));   echo "PASS   $1" >> "$LOG"; }
fail()   { echo -e "${RED}  ✗ FAIL${RST}  $1  ← $2"; FAIL=$((FAIL+1)); echo "FAIL   $1  [$2]" >> "$LOG"; }
skip()   { echo -e "${YLW}  - SKIP${RST}  $1  ($2)"; SKIP=$((SKIP+1));  echo "SKIP   $1  [$2]" >> "$LOG"; }
manual() { echo -e "${CYN}  ? MANUAL${RST} $1  → $2"; MANUAL=$((MANUAL+1)); echo "MANUAL $1  [$2]" >> "$LOG"; }
detail() { echo "        $1"; echo "       $1" >> "$LOG"; }

section() {
    [[ -n "$FILTER" && "$FILTER" != "$1" ]] && return 1
    echo ""; echo -e "${BLD}══ $2 ══${RST}"
    return 0
}

chk_prop() {
    _val=$(adb_sh getprop "$2")
    if [[ -n "$3" && "$_val" == *"$3"* ]]; then pass "$1 ($3)"; \
    elif [[ -z "$3" && -n "$_val" ]]; then pass "$1 ($2=$_val)"; \
    else fail "$1" "$2='$_val'"; fi
}
chk_proc() {
    if adb_sh "ps" | grep -v grep | grep -q "$2"; then pass "$1"; \
    else fail "$1" "'$2' no encontrado en ps"; fi
}
chk_file() {
    if adb_sh "[ -e '$2' ] && echo y" | grep -q y; then pass "$1 ($2)"; \
    else fail "$1" "no existe: $2"; fi
}
chk_svc() {
    _st=$(adb_sh getprop "init.svc.$1")
    if [[ "$_st" == "running" ]]; then pass "servicio $1 running"; \
    else fail "servicio $1" "init.svc.$1='$_st'"; fi
}

# ── 0. zip (en el PC) ─────────────────────────────────────────────────────────

# Una build cortada (OOM) dejó LatinIME.apk truncado con fecha nueva; la build
# siguiente lo empaquetó y tras un wipe no hubo teclado.
if [[ -n "$ZIP" ]] && section zip "ZIP (antes de flashear)"; then
    if [[ ! -f "$ZIP" ]]; then
        fail "Zip" "no existe: $ZIP"
    else
        _zd=$(mktemp -d)
        unzip -q "$ZIP" 'system/app/*' 'system/framework/*' -d "$_zd" 2>/dev/null
        _bad=""; _n=0
        for f in "$_zd"/system/app/*.apk "$_zd"/system/framework/*.jar "$_zd"/system/framework/*.apk; do
            [[ -f "$f" ]] || continue
            _n=$((_n+1))
            unzip -tq "$f" >/dev/null 2>&1 && continue
            [[ " $KNOWN_BAD_APKS " == *" $(basename "$f") "* ]] && continue
            _bad="$_bad $(basename "$f")"
        done
        rm -rf "$_zd"
        if [[ -z "$_bad" ]]; then pass "$_n apk/jar del zip íntegros"; \
        else fail "apk/jar rotos en el zip" "$_bad"; fi
    fi
fi
[[ "$FILTER" == "zip" ]] && { echo ""; echo "PASS: $PASS  FAIL: $FAIL"; exit $((FAIL > 0)); }

# ── pre-check ─────────────────────────────────────────────────────────────────

echo -e "${BLD}test_y220.sh — Huawei Y220 CM7 smoke test + caza de bugs${RST}"
echo "Log: $LOG"

if ! adb devices | grep -q "device$"; then
    echo -e "${RED}ERROR: no hay dispositivo ADB conectado.${RST}"; exit 1
fi

_model=$(adb_sh getprop ro.product.model)
echo -e "Dispositivo: ${BLD}$_model${RST}  |  Fecha: $(date)"
echo "Dispositivo: $_model" >> "$LOG"

# logcat y dmesg del arranque actual, una sola vez para todas las secciones
LC=$(mktemp); DM=$(mktemp)
trap 'rm -f "$LC" "$DM"' EXIT
adb_sh "logcat -v time -d" > "$LC"
adb_sh "dmesg" > "$DM"
_up=$(adb_sh "cut -d. -f1 /proc/uptime")
_now=$(adb_sh "date +%s")
_boot=$((_now - _up))

# ── 1. boot / sistema ─────────────────────────────────────────────────────────

if section boot "BOOT / SISTEMA"; then
    chk_prop  "Boot completado"           sys.boot_completed       "1"
    chk_prop  "Versión Android 2.3"       ro.build.version.release "2.3"
    chk_prop  "ro.product.device y220"    ro.product.device        "y220"
    chk_prop  "ro.hardware mt6572"        ro.hardware              "mt6572"
    chk_proc  "system_server"             "system_server"
    chk_proc  "mediaserver"               "mediaserver"
    for s in servicemanager vold netd zygote media dbus ccci_fsd ccci_mdinit \
             gsm0710muxd ril-daemon mtkbt; do
        chk_svc "$s"
    done
    pass "Uptime ${_up}s"
fi

# ── 2. caza de bugs (arranque actual) ─────────────────────────────────────────

if section bugs "CAZA DE BUGS (arranque actual)"; then
    # Reinicios del framework: una línea ZygoteInit por arranque de zygote.
    _zy=$(grep -c "START com.android.internal.os.ZygoteInit" "$LC")
    if [[ "$_zy" -le 1 ]]; then pass "Zygote/system_server sin reinicios (logcat: $_zy)"; \
    else fail "Zygote reiniciado $_zy veces" "bootloop o crash de system_server"; fi

    _fx=$(grep -c "FATAL EXCEPTION" "$LC")
    if [[ "$_fx" -eq 0 ]]; then pass "Sin FATAL EXCEPTION Java"; \
    else
        fail "FATAL EXCEPTION Java: $_fx" "ver detalle"
        grep -A2 "FATAL EXCEPTION" "$LC" | grep -E "Exception|Error" | sort | uniq -c | head -5 |
            while read -r l; do detail "$l"; done
    fi

    _sig=$(grep -E "Fatal signal|pid: [0-9]+, tid: [0-9]+ +>>> .* <<<" "$LC")
    if [[ -z "$_sig" ]]; then pass "Sin crashes nativos en logcat"; \
    else
        fail "Crashes nativos" "$(echo "$_sig" | grep -c .) líneas"
        echo "$_sig" | grep -o ">>> .* <<<" | sort | uniq -c | while read -r l; do detail "$l"; done
    fi

    # Tombstones nuevos desde el arranque (los viejos se ignoran).
    _tn=0
    for t in $(adb_sh "ls /data/tombstones 2>/dev/null"); do
        _ts=$(adb_sh "busybox date -r /data/tombstones/$t +%s")
        [[ -n "$_ts" && "$_ts" -ge "$_boot" ]] || continue
        _tn=$((_tn+1))
        detail "$t: $(adb_sh "grep -m1 '>>>' /data/tombstones/$t") $(adb_sh "grep -m1 '^signal' /data/tombstones/$t")"
    done
    if [[ "$_tn" -eq 0 ]]; then pass "Sin tombstones nuevos"; \
    else fail "Tombstones nuevos: $_tn" "adb pull /data/tombstones"; fi

    _anr=$(grep -c "ANR in" "$LC")
    if [[ "$_anr" -eq 0 ]]; then pass "Sin ANR"; \
    else
        fail "ANR: $_anr" "ver /data/anr/traces.txt"
        grep "ANR in" "$LC" | sed 's/.*ANR in //' | sort | uniq -c | while read -r l; do detail "$l"; done
    fi

    _died=$(grep -E "Process .* has died|died: " "$LC" | grep -v "lowmem\|kill" )
    if [[ -z "$_died" ]]; then pass "Sin procesos muertos inesperadamente"; \
    else
        skip "Procesos muertos: $(echo "$_died" | grep -c .)" "revisar si son esperados"
        echo "$_died" | sed 's/.*Process //' | awk '{print $1}' | sort | uniq -c | head -5 |
            while read -r l; do detail "$l"; done
    fi

    # Servicios nativos que init reinicia (crash loop de daemons). "untracked
    # pid exited" son hijos de daemons, normal.
    _rs=$(grep -E "init: .*(exited|died|restarting|critical)" "$DM" | grep -v "bootanim\|untracked pid")
    if [[ -z "$_rs" ]]; then pass "init: sin servicios caídos"; \
    else
        fail "init: servicios caídos/reiniciados" "$(echo "$_rs" | grep -c .) eventos"
        echo "$_rs" | grep -o "'[^']*'" | sort | uniq -c | while read -r l; do detail "$l"; done
    fi

    _lib=$(grep -E "Cannot load library|dlopen failed|couldn't find .*\.so|link_image.*failed" "$LC" | sed 's/^[^:]*: //' | sort -u)
    if [[ -z "$_lib" ]]; then pass "Sin librerías faltantes"; \
    else
        fail "Librerías faltantes/sin símbolos" "$(echo "$_lib" | grep -c .)"
        echo "$_lib" | head -5 | while read -r l; do detail "$l"; done
    fi

    # Un apk de /system/app que PackageManager no registró está roto (así quedó
    # LatinIME.apk tras una build cortada).
    _reg=$(adb_sh "pm list packages -f" | sed 's/^package://; s/=.*//')
    _unreg=""
    for a in $(adb_sh "ls /system/app" | grep '\.apk$'); do
        [[ " $KNOWN_BAD_APKS " == *" $a "* ]] && continue
        echo "$_reg" | grep -qx "/system/app/$a" || _unreg="$_unreg $a"
    done
    if [[ -z "$_unreg" ]]; then pass "Todos los apk de /system/app registrados"; \
    else fail "apk de /system/app sin registrar" "$_unreg"; fi

    _ime=$(adb_sh "sqlite3 /data/data/com.android.providers.settings/databases/settings.db \"select value from secure where name='default_input_method';\"")
    if [[ -n "$_ime" ]] && adb_sh "pm list packages" | grep -qx "package:${_ime%%/*}"; then
        pass "Teclado predeterminado ($_ime)"
    else
        fail "Teclado" "default_input_method='$_ime' sin paquete instalado"
    fi

    # UnknownHostException antes de que suba la red es normal; el bug era
    # SocketException (host nulo -> loopback IPv6, sin IPv6 en el kernel).
    _ntp=$(grep "SntpClient.*request time failed" "$LC" | grep -vc "UnknownHostException")
    if [[ "$_ntp" -eq 0 ]]; then pass "NTP sin fallos de socket"; \
    else fail "NTP falló $_ntp veces" "¿falta /system/etc/gps.conf (NTP_SERVER)?"; fi

    detail "Tags con más errores (E/):"
    grep -E "^[0-9-]+ [0-9:.]+ E/" "$LC" | sed -E 's/^[^E]*E\/([^(]*)\(.*/\1/' | sort | uniq -c |
        sort -rn | head -8 | while read -r l; do detail "  $l"; done
fi

# ── 3. pantalla / gráficos ────────────────────────────────────────────────────

if section display "PANTALLA / GRÁFICOS"; then
    chk_file "Framebuffer fb0"      "/dev/graphics/fb0"
    chk_file "gralloc.mt6572.so"    "/system/lib/hw/gralloc.mt6572.so"
    chk_file "mtkfb vsync"          "/dev/mtkfb_vsync"
    if adb_sh "service list" | grep -qi surfaceflinger; then pass "surfaceflinger registrado"; \
    else fail "surfaceflinger" "no en service list"; fi
    manual "Touchscreen"  "toca y desliza — respuesta fluida"
fi

# ── 4. audio ──────────────────────────────────────────────────────────────────

if section audio "AUDIO"; then
    chk_file "libaudio.so"        "/system/lib/libaudio.so"
    chk_file "libaudiopolicy.so"  "/system/lib/libaudiopolicy.so"
    if adb_sh "service list" | grep -q "media.audio_flinger"; then pass "AudioFlinger registrado"; \
    else fail "AudioFlinger" "no en service list"; fi
    if adb_sh "service list" | grep -q "media.audio_policy"; then pass "AudioPolicy registrado"; \
    else fail "AudioPolicy" "no en service list"; fi
    manual "Speaker / auriculares / micrófono"  "reproduce, conecta auriculares y graba"
    manual "Llamada audio"  "audio subida y bajada"
fi

# ── 5. Wi-Fi ──────────────────────────────────────────────────────────────────

if section wifi "WI-FI"; then
    chk_file "/dev/wmtWifi"  "/dev/wmtWifi"
    chk_file "Firmware WIFI_RAM_CODE" "/system/etc/firmware/WIFI_RAM_CODE_MT6582"
    if adb_sh "lsmod" | grep -q wlan_mt6582; then pass "módulo wlan_mt6582 cargado"; \
    else skip "módulo wlan_mt6582" "Wi-Fi apagado"; fi
    if adb_sh "ip link" | grep -q wlan0; then pass "Interfaz wlan0"; \
    else skip "wlan0" "Wi-Fi apagado"; fi
    _wstat=$(adb_sh getprop wlan.driver.status)
    if [[ "$_wstat" == "ok" ]]; then pass "wlan.driver.status=ok"; \
    else skip "wlan.driver.status" "='$_wstat'"; fi
    _ip=$(adb_sh getprop dhcp.wlan0.ipaddress)
    if [[ -n "$_ip" ]]; then pass "DHCP wlan0 $_ip"; else skip "DHCP wlan0" "sin red asociada"; fi
fi

# ── 6. Bluetooth ──────────────────────────────────────────────────────────────

if section bt "BLUETOOTH"; then
    chk_file "/dev/stpbt"          "/dev/stpbt"
    chk_file "libbt_mtk_gap.so"    "/system/lib/libbt_mtk_gap.so"
    chk_svc  "mtkbt"
    _on=$(adb_sh "sqlite3 /data/data/com.android.providers.settings/databases/settings.db \"select value from secure where name='bluetooth_on'\"")
    if [[ "$_on" != "1" ]]; then
        skip "BT encendido" "bluetooth_on='$_on' — encender desde ajustes y re-ejecutar"
    else
        pass "BT encendido (bluetooth_on=1)"
        if grep -q "cnf 145 result=[01]" "$LC"; then pass "POWERON_CNF ok"; \
        else fail "POWERON" "sin cnf 145 en logcat"; fi
        if grep -q "cnf 159 result=0" "$LC"; then pass "Clase de dispositivo escrita (cnf 159)"; \
        else fail "WRITE_LOCAL_COD" "sin cnf 159 result=0"; fi
        _nm=$(grep "WRITE_LOCAL_NAME_REQ" "$LC" | tail -1 | sed 's/.*REQ //')
        if [[ -n "$_nm" ]] && grep -q "cnf 151 result=0" "$LC"; then pass "Nombre local $_nm"; \
        else fail "WRITE_LOCAL_NAME" "sin cnf 151 result=0"; fi
    fi
    _bad=$(grep "bt_mtk_gap" "$LC" | grep -E "timeout|cnf [0-9]+ result=([2-9]|[1-9][0-9])|send .*: |bind ")
    if [[ -z "$_bad" ]]; then pass "bt_mtk_gap sin timeouts ni CNF con error"; \
    else
        fail "bt_mtk_gap: errores" "$(echo "$_bad" | grep -c .)"
        echo "$_bad" | sed 's/^[^:]*: //' | sort | uniq -c | head -5 | while read -r l; do detail "$l"; done
    fi
    if grep -q "bt_mtk_a2dp.*A2DP activate sent" "$LC"; then pass "A2DP activado en mtkbt"; \
    else fail "A2DP" "sin 'A2DP activate sent' (libbt_mtk_a2dp)"; fi
    # Perfiles BlueZ (OPP, SCO, RFCOMM, PAN) abren sockets AF_BLUETOOTH; el
    # kernel MTK no tiene BlueZ, mtkbt lleva los perfiles. Los servidores de
    # AOSP que reintentan sin socket son conocidos: SCO (manos libres) y PBAP.
    # El listener OPP ya no corre (recibe mtkbt); otro componente es nuevo.
    _af=$(grep -E "Address family not supported|Pan operation failed" "$LC" | grep " E/" | sed -E 's/^[^E]*E\/([^(]*)\(.*/\1/' | sort | uniq -c)
    _af_new=$(echo "$_af" | grep -vE "bluetooth_ScoSocket.cpp|BluetoothPbapService" | grep .)
    if [[ -z "$_af_new" ]]; then
        pass "Perfiles BT: solo los servidores BlueZ conocidos sin socket"
        [[ -n "$_af" ]] && echo "$_af" | while read -r l; do detail "conocido: $l"; done
    else
        fail "Perfiles BT (sockets BlueZ)" "AF_BLUETOOTH en un componente nuevo"
        echo "$_af_new" | while read -r l; do detail "$l"; done
    fi
    _early=$(grep -c "Cannot broadcast before boot completed" "$LC")
    [[ "$_early" -gt 0 ]] && detail "avisos tempranos atrapados (no fatales): $_early"
    manual "Visible"     "toca 'visible', espera 10 s y busca el Y220 desde otro celular"
    manual "Pairing"     "empareja con otro celular (confirmación de código)"
    manual "A2DP / OPP"  "audio a parlante BT y envío de un archivo"
fi

# ── 7. radio FM ───────────────────────────────────────────────────────────────

if section fm "RADIO FM"; then
    chk_file "libfmjni.so"     "/system/lib/libfmjni.so"
    chk_file "libfmmt6627.so"  "/system/lib/libfmmt6627.so"
    chk_file "firmware mt6627" "/system/etc/firmware/mt6627"
    if adb_sh "[ -e /dev/fm ] && echo y" | grep -q y; then pass "/dev/fm"; \
    else skip "/dev/fm" "mtk_fm_drv no cargado (FM aún no portado)"; fi
fi

# ── 8. sensores ───────────────────────────────────────────────────────────────

if section sensors "SENSORES"; then
    chk_file "/dev/gsensor"   "/dev/gsensor"
    chk_file "/dev/hwmsensor" "/dev/hwmsensor"
    if adb_sh "dumpsys sensorservice" | grep -qi "ACCELEROMETER"; then pass "Acelerómetro en SensorService"; \
    else fail "Acelerómetro SensorService" "no aparece en dumpsys"; fi
    manual "Rotación" "gira el teléfono — la pantalla rota"
fi

# ── 9. RIL / telefonía ────────────────────────────────────────────────────────

if section ril "RIL / TELEFONÍA"; then
    chk_file "/dev/ccci_ioctl0" "/dev/ccci_ioctl0"
    _bb=$(adb_sh getprop gsm.version.baseband)
    if [[ -n "$_bb" ]]; then pass "Baseband: $_bb"; else fail "Baseband" "gsm.version.baseband vacío"; fi
    _sim=$(adb_sh getprop gsm.sim.state)
    _op=$(adb_sh getprop gsm.operator.alpha)
    if [[ -n "$_op" ]]; then pass "Operador: $_op"; \
    elif [[ "$_sim" == "ABSENT" ]]; then skip "Operador" "sin SIM"; \
    else fail "Operador" "vacío (sim.state=$_sim)"; fi
    if adb_sh "ip link" | grep -q "ccmni0.*UP"; then pass "ccmni0 arriba (datos)"; \
    else skip "ccmni0" "datos móviles apagados"; fi
    manual "Llamadas / SMS" "saliente y entrante"
fi

# ── 10. almacenamiento ────────────────────────────────────────────────────────

if section storage "ALMACENAMIENTO"; then
    for m in /system /data /cache; do
        _df=$(adb_sh "df $m | tail -1")
        if [[ -n "$_df" ]]; then pass "$m montado ($_df)"; else fail "$m" "df falló"; fi
    done
    if adb_sh "mount" | grep -q "/mnt/sdcard"; then pass "SDCard montada"; \
    else skip "SDCard" "no montada"; fi
fi

# ── 11. energía ───────────────────────────────────────────────────────────────

if section power "ENERGÍA"; then
    _batt=$(adb_sh "cat /sys/class/power_supply/battery/capacity")
    if [[ -n "$_batt" && "$_batt" -gt 0 ]] 2>/dev/null; then pass "Batería: ${_batt}%"; \
    else fail "Batería sysfs" "capacity='$_batt'"; fi
    if $FAST; then skip "Suspensión" "--fast: omitido"; \
    else manual "Deep sleep" "desconecta USB 5 min — Wi-Fi/BT siguen funcionando al volver"; fi
fi

# ── resumen ────────────────────────────────────────────────────────────────────

TOTAL=$((PASS+FAIL+SKIP+MANUAL))
echo ""; echo -e "${BLD}══ RESUMEN ══${RST}"
printf "  Total:    %d\n" "$TOTAL"
echo -e "  ${GRN}PASS:     $PASS${RST}"
echo -e "  ${RED}FAIL:     $FAIL${RST}"
echo -e "  ${YLW}SKIP:     $SKIP${RST}"
echo -e "  ${CYN}MANUAL:   $MANUAL${RST}"
echo ""
echo "Log: $LOG"
{ echo ""; echo "RESUMEN: PASS=$PASS FAIL=$FAIL SKIP=$SKIP MANUAL=$MANUAL / $TOTAL"; } >> "$LOG"

exit $FAIL
