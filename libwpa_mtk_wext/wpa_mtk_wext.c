#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/wireless.h>
#include <net/if.h>

#include "wpa_private.h"

/* gl_wext.c: SIOCSIWPRIV is wext_set_country(), and it reads bytes 8-9. */

#ifdef MTK_WEXT_COUNTRY
static void mtk_wext_set_country(const char *ifname)
{
	struct iwreq iwr;
	char buf[16];
	int s;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		return;
	memset(buf, 0, sizeof(buf));
	memcpy(buf, "COUNTRY ", 8);
	buf[8] = MTK_WEXT_COUNTRY[0];
	buf[9] = MTK_WEXT_COUNTRY[1];
	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name) - 1);
	iwr.u.data.pointer = buf;
	iwr.u.data.length = sizeof(buf);
	ioctl(s, SIOCSIWPRIV, &iwr);
	close(s);
}
#endif

void wpa_private_on_driver_start(const char *ifname)
{
	struct ifreq ifr;
	int s;

	/* DRIVER STOP clears IFF_UP. DRIVER START does not set it again,
	 * and a scan on a down iface returns no BSSes. */
	if (ifname == NULL || ifname[0] == '\0')
		return;
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		return;
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);
	if (ioctl(s, SIOCGIFFLAGS, &ifr) == 0 && !(ifr.ifr_flags & IFF_UP)) {
		ifr.ifr_flags |= IFF_UP;
		ioctl(s, SIOCSIFFLAGS, &ifr);
	}
	close(s);
#ifdef MTK_WEXT_COUNTRY
	/* NVRAM has no country. Without one the scan OID returns no BSSes. */
	mtk_wext_set_country(ifname);
#endif
}

void wpa_private_before_scan(int *disconnected)
{
	/* A UI SCAN must not stay on the explicit-disconnect flag.
	 * Otherwise the completion is reported as WPA_IDLE. */
	if (disconnected)
		*disconnected = 0;
}

int wpa_private_report_idle(int has_ssid, int has_bssid)
{
	/* disconnected is only set by DISCONNECT. Reporting that as
	 * WPA_IDLE makes Android drop wlan0 when nothing was associated. */
	return has_ssid || has_bssid;
}

int wpa_private_rescan_on_query(void)
{
	/* An empty cache from a scan that had not finished yet would
	 * otherwise stick and Settings would keep an empty list. */
	return 1;
}

int wpa_private_do_priv_ioctl(const char *cmd)
{
	/* Only a real COUNTRY command may use SIOCSIWPRIV. SCAN-ACTIVE
	 * becomes country "IV", SCAN-PASSIVE becomes "SI" and the CSCAN
	 * header becomes 00, so the firmware scans no channels. */
	return cmd && strncmp(cmd, "COUNTRY ", 8) == 0;
}

int wpa_private_hide_scan_bss(const unsigned char *bssid,
			      const unsigned char *ssid, int ssid_len)
{
	static const char prefix[] = "NVRAM WARNING";
	int i;

	/* wlanCheckSystemConfiguration() injects this when the NVRAM
	 * read fails. It is not a BSS. */
	if (ssid && ssid_len >= (int)sizeof(prefix) - 1 &&
	    memcmp(ssid, prefix, sizeof(prefix) - 1) == 0)
		return 1;
	if (bssid) {
		for (i = 0; i < 6; i++)
			if (bssid[i])
				return 0;
		return 1;
	}
	return 0;
}
