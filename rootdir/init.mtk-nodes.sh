#!/system/bin/sh
# Device nodes of the MTK drivers, run once by init (exec) from
# init.connectivity.rc after every module in init.*.rc is loaded.
#
# These drivers register no class device, so ueventd never makes their
# nodes; stock init.rc made them with mknod, which CM's init does not have.
# Owners use busybox chown: toolbox chown only takes names, and the ccci
# (9996) and nvram (9997) ids have none in CM.
BB=/system/xbin/busybox

node() { # path major minor mode owner
    [ -c $1 ] || $BB mknod $1 c $2 $3
    $BB chmod $4 $1
    $BB chown $5 $1
}

# Modem and security (stock init.rc, init.modem.rc).
node /dev/sec 182 0 0660 0:1000
node /dev/devmap 196 0 0444 0:1013

# Video codec (vcodec_kernel_driver.ko).
node /dev/Vcodec 160 0 0660 1000:1013

# Combo chip: WMT, GPS, BT, Wi-Fi, FM (stock init.project.rc).
node /dev/stpwmt 190 0 0660 1000:1000
node /dev/stpgps 191 0 0660 1021:1021
node /dev/stpbt 192 0 0660 1002:3008
node /dev/wmtWifi 153 0 0660 1000:1000
node /dev/fm 193 0 0660 1000:1013

# ccci makes its nodes through ueventd as root 0600 once ccci.ko is in.
i=0
while [ ! -c /dev/ttyC0 ] && [ $i -lt 30 ]; do sleep 0.1; i=$((i+1)); done
$BB chown 9996:9996 /dev/ccci* /dev/ttyC*
$BB chmod 660 /dev/ccci* /dev/ttyC*
# Voice call PCM: the audio HAL in mediaserver (group audio) talks to the
# modem here.
$BB chown 9996:1005 /dev/ccci_pcm_rx /dev/ccci_pcm_tx
$BB chown 9996:9996 /sys/kernel/ccci/boot
if [ -c /dev/otp ]; then
    $BB chown 0:9997 /dev/otp
    $BB chmod 660 /dev/otp
fi
