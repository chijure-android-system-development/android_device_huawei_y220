/*
 * Android "DRIVER" commands for wlan_mt6582.ko over nl80211.
 *
 * The 4.2 framework sends START/STOP and MACADDR, and a few tuning
 * commands (POWERMODE, BTCOEXMODE, RXFILTER-*, SETSUSPENDOPT, COUNTRY)
 * that the MTK driver does not take as Android private ioctls. Those are
 * accepted and ignored; failing them only fills the log.
 *
 * Based on hardware/atheros/wlan/ath6kl/wpa_supplicant_8_lib.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 */

#include "driver_nl80211.h"
#include "wpa_supplicant_i.h"
#include "config.h"
#ifdef ANDROID
#include "android_drv.h"
#endif

int wpa_driver_nl80211_driver_cmd(void *priv, char *cmd, char *buf,
				  size_t buf_len)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ret = 0;

	if (os_strcasecmp(cmd, "STOP") == 0) {
		linux_set_iface_flags(drv->global->ioctl_sock, bss->ifname, 0);
		wpa_msg(drv->ctx, MSG_INFO, WPA_EVENT_DRIVER_STATE "STOPPED");
	} else if (os_strcasecmp(cmd, "START") == 0) {
		linux_set_iface_flags(drv->global->ioctl_sock, bss->ifname, 1);
		wpa_msg(drv->ctx, MSG_INFO, WPA_EVENT_DRIVER_STATE "STARTED");
	} else if (os_strcasecmp(cmd, "MACADDR") == 0) {
		u8 macaddr[ETH_ALEN] = {};

		ret = linux_get_ifhwaddr(drv->global->ioctl_sock, bss->ifname,
					 macaddr);
		if (!ret)
			ret = os_snprintf(buf, buf_len, "Macaddr = " MACSTR "\n",
					  MAC2STR(macaddr));
	} else if (os_strcasecmp(cmd, "RELOAD") == 0) {
		wpa_msg(drv->ctx, MSG_INFO, WPA_EVENT_DRIVER_STATE "HANGED");
	} else {
		wpa_printf(MSG_DEBUG, "%s: ignoring %s", __func__, cmd);
	}
	return ret;
}
