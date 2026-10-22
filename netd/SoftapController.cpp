/*
 * Wi-Fi hotspot for the MT6572 combo (wlan_mt6582.ko).
 *
 * The driver makes its AP interface on request: the set_p2p_mode private
 * ioctl on wlan0 (enable 1, mode 1 = AP) registers ap0, a cfg80211-only
 * netdev. hostapd 0.8 runs it over nl80211 (device/huawei/y220/hostapd).
 * The softap interface name the framework passes (wl0.1) is ignored.
 * hostapd 0.8 switches itself to the wifi uid at start (os_program_init),
 * so its config and control directory belong to wifi.
 *
 * Command order from NetworkManagementService.startAccessPoint:
 *   softap stop wlan0; softap fwreload wlan0 AP; softap start wlan0;
 *   softap set wlan0 wl0.1 <ssid> <open|wpa2-psk> <key>; softap startap
 * and stopAccessPoint: softap stopap; softap stop wlan0.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sched.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <linux/wireless.h>

#define LOG_TAG "SoftapController"
#include <cutils/log.h>
#include <cutils/properties.h>
#include <private/android_filesystem_config.h>

extern "C" int ifc_init(void);
extern "C" void ifc_close(void);
extern "C" int ifc_up(const char *name);
extern "C" int ifc_down(const char *name);

#include "SoftapController.h"

#define AP_IFACE            "ap0"
#define HOSTAPD_NAME        "hostapd"
#define HOSTAPD_PROP        "init.svc.hostapd"
#define HOSTAPD_CONF        "/data/misc/wifi/hostapd.conf"
#define HOSTAPD_CTRL_DIR    "/data/misc/wifi/hostapd"
#define DEFAULT_SSID        "AndroidAP"
#define DEFAULT_CHANNEL     6

/* wlan_mt6582 gl_wext_priv.h: PRIV_CMD_P2P_MODE, a sub-ioctl of
 * IOCTL_SET_INT (SIOCIWFIRSTPRIV + 0) with two fixed ints. */
#define MTK_PRIV_CMD_P2P_MODE   28

static bool hostapdRunning() {
    char status[PROPERTY_VALUE_MAX];
    return property_get(HOSTAPD_PROP, status, NULL) && !strcmp(status, "running");
}

static int startHostapd() {
    char status[PROPERTY_VALUE_MAX];
    int count = 100; /* 10 s */

    if (hostapdRunning())
        return 0;
    property_set("ctl.start", HOSTAPD_NAME);
    sched_yield();
    while (count-- > 0) {
        if (property_get(HOSTAPD_PROP, status, NULL) && !strcmp(status, "running"))
            return 0;
        usleep(100000);
    }
    LOGE("hostapd did not start");
    return -1;
}

static int stopHostapd() {
    char status[PROPERTY_VALUE_MAX];
    int count = 50; /* 5 s */

    if (property_get(HOSTAPD_PROP, status, NULL) && !strcmp(status, "stopped"))
        return 0;
    property_set("ctl.stop", HOSTAPD_NAME);
    sched_yield();
    while (count-- > 0) {
        if (property_get(HOSTAPD_PROP, status, NULL) && !strcmp(status, "stopped"))
            return 0;
        usleep(100000);
    }
    LOGE("hostapd did not stop");
    return -1;
}

static bool apIfaceExists() {
    return access("/sys/class/net/" AP_IFACE, F_OK) == 0;
}

SoftapController::SoftapController() {
    mPid = 0;
    mSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (mSock < 0)
        LOGE("Failed to open socket");
    memset(mIface, 0, sizeof(mIface));
    strlcpy(mIface, "wlan0", sizeof(mIface));
}

SoftapController::~SoftapController() {
    if (mSock >= 0)
        close(mSock);
}

/* Unused: kept for the shared header. */
int SoftapController::getPrivFuncNum(char *iface, const char *fname) {
    return -1;
}

int SoftapController::addParam(int pos, const char *cmd, const char *arg) {
    return pos;
}

static int setP2pMode(int sock, const char *iface, int enable, int mode) {
    struct iwreq wrq;
    int *args = (int *)wrq.u.name;

    memset(&wrq, 0, sizeof(wrq));
    strlcpy(wrq.ifr_name, iface, sizeof(wrq.ifr_name));
    args[0] = MTK_PRIV_CMD_P2P_MODE;
    args[1] = enable;
    args[2] = mode;
    if (ioctl(sock, SIOCIWFIRSTPRIV, &wrq) < 0) {
        LOGE("set_p2p_mode %d %d on %s: %s", enable, mode, iface, strerror(errno));
        return -1;
    }
    return 0;
}

/* WifiStateTracker.loadDriver already loaded the driver and powered WMT. */
int SoftapController::startDriver(char *iface) {
    if (iface && iface[0])
        strlcpy(mIface, iface, sizeof(mIface));
    LOGD("Softap driver start (%s)", mIface);
    return 0;
}

int SoftapController::stopDriver(char *iface) {
    LOGD("Softap driver stop");
    return 0;
}

int SoftapController::fwReloadSoftap(int argc, char *argv[]) {
    return 0;
}

int SoftapController::startSoftap() {
    int count;

    if (mSock < 0) {
        LOGE("Softap startap - failed to open socket");
        return -1;
    }
    if (hostapdRunning()) {
        LOGD("Softap already started");
        return 0;
    }

    ifc_init();
    ifc_up(mIface);
    ifc_close();

    if (!apIfaceExists() && setP2pMode(mSock, mIface, 1, 1) < 0)
        return -1;
    for (count = 0; count < 20 && !apIfaceExists(); count++)
        usleep(100000);
    if (!apIfaceExists()) {
        LOGE("Softap startap - %s did not appear", AP_IFACE);
        return -1;
    }

    /* A bad config makes hostapd exit right after init marks it running. */
    if (startHostapd() < 0 || (usleep(1000000), !hostapdRunning())) {
        LOGE("Softap startap - hostapd failed");
        setP2pMode(mSock, mIface, 0, 0);
        return -1;
    }
    LOGD("Softap startap - Ok");
    return 0;
}

int SoftapController::stopSoftap() {
    stopHostapd();
    if (apIfaceExists() && mSock >= 0)
        setP2pMode(mSock, mIface, 0, 0);
    LOGD("Softap service stopped");
    return 0;
}

bool SoftapController::isSoftapStarted() {
    return hostapdRunning();
}

/*
 * argv[2] - wlan interface
 * argv[3] - softap interface (ignored, the driver names it ap0)
 * argv[4] - SSID
 * argv[5] - security: open, wpa-psk or wpa2-psk
 * argv[6] - key
 * argv[7] - channel
 */
int SoftapController::setSoftap(int argc, char *argv[]) {
    const char *ssid = DEFAULT_SSID;
    const char *sec = "open";
    const char *key = NULL;
    int channel = DEFAULT_CHANNEL;
    bool restart;
    FILE *f;

    if (argc < 4) {
        LOGE("Softap set - missing arguments");
        return -1;
    }
    strlcpy(mIface, argv[2], sizeof(mIface));
    if (argc > 4)
        ssid = argv[4];
    if (argc > 5)
        sec = argv[5];
    if (argc > 6)
        key = argv[6];
    if (argc > 7 && atoi(argv[7]) >= 1 && atoi(argv[7]) <= 13)
        channel = atoi(argv[7]);

    mkdir(HOSTAPD_CTRL_DIR, 0770);
    chown(HOSTAPD_CTRL_DIR, AID_WIFI, AID_WIFI);
    f = fopen(HOSTAPD_CONF, "w");
    if (f == NULL) {
        LOGE("Softap set - cannot write %s: %s", HOSTAPD_CONF, strerror(errno));
        return -1;
    }
    fprintf(f, "interface=" AP_IFACE "\n"
               "driver=nl80211\n"
               "ctrl_interface=" HOSTAPD_CTRL_DIR "\n"
               "ssid=%s\n"
               "channel=%d\n"
               "hw_mode=g\n"
               "ignore_broadcast_ssid=0\n", ssid, channel);
    if (key != NULL && strstr(sec, "wpa") != NULL) {
        fprintf(f, "wpa=%d\n"
                   "wpa_key_mgmt=WPA-PSK\n"
                   "wpa_pairwise=CCMP\n"
                   "rsn_pairwise=CCMP\n"
                   "wpa_passphrase=%s\n",
                strcmp(sec, "wpa-psk") ? 2 : 1, key);
    }
    fclose(f);
    chown(HOSTAPD_CONF, AID_WIFI, AID_WIFI);
    chmod(HOSTAPD_CONF, 0660);
    LOGD("Softap set - SSID %s, %s, channel %d", ssid, sec, channel);

    /* A running AP picks the new settings up on restart. */
    restart = hostapdRunning();
    if (restart) {
        stopHostapd();
        return startHostapd();
    }
    return 0;
}
