#ifndef WPA_PRIVATE_H
#define WPA_PRIVATE_H

/* Implemented by BOARD_WPA_SUPPLICANT_PRIVATE_LIB (libwpa_mtk_wext). */

void wpa_private_on_driver_start(const char *ifname);
void wpa_private_before_scan(int *disconnected);

/* Non-zero: report WPA_DISCONNECTED. Android maps that to WPA_IDLE
 * while the disconnected flag is set. Zero: keep the no-association path. */
int wpa_private_report_idle(int has_ssid, int has_bssid);

/* Non-zero: re-read the driver on every SCAN_RESULTS query. */
int wpa_private_rescan_on_query(void);

/* Non-zero: issue SIOCSIWPRIV. On MT6620 that ioctl is the country
 * code, so SCAN-ACTIVE / CSCAN must not be sent as-is. */
int wpa_private_do_priv_ioctl(const char *cmd);

/* Non-zero: omit this BSS from SCAN_RESULTS. */
int wpa_private_hide_scan_bss(const unsigned char *bssid,
			      const unsigned char *ssid, int ssid_len);

#endif
