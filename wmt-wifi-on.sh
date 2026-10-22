#!/system/bin/sh
# Called by the generic CM7 WIFI_FIRMWARE_LOADER path after insmod. WMT
# function-on creates wlan0; report completion through the property that
# hardware/libhardware_legacy already waits for.
if [ -d /sys/class/net/wlan0 ]; then
    setprop wlan.driver.status ok
    exit 0
fi
echo 0 > /dev/wmtWifi
echo 1 > /dev/wmtWifi

COUNT=4
while [ "$COUNT" -gt 0 ]; do
    if [ -d /sys/class/net/wlan0 ]; then
        setprop wlan.driver.status ok
        exit 0
    fi
    sleep 1
    COUNT=$((COUNT - 1))
done

setprop wlan.driver.status failed
exit 1
