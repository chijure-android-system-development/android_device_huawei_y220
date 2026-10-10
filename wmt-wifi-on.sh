#!/system/bin/sh
# insmod does not create wlan0. WMT function-on does, and only if the
# function is not already open.
[ -d /sys/class/net/wlan0 ] && exit 0
echo 0 > /dev/wmtWifi
echo 1 > /dev/wmtWifi
