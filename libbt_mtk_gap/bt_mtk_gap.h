#ifndef BT_MTK_GAP_H
#define BT_MTK_GAP_H

#ifdef __cplusplus
extern "C" {
#endif

/* 0: POWERON_CNF / POWEROFF_CNF result is 0 or 1. -1: no reply. */
int btmtk_gap_power_on(void);
int btmtk_gap_power_off(void);
/* "AA:BB:CC:DD:EE:FF" from /data/BT_Addr. 0 on success. */
int btmtk_gap_local_address(char *out, int outlen);

/* Discovery. start/cancel return 0 when the request went out. */
int btmtk_gap_discovery_start(void);
int btmtk_gap_discovery_cancel(void);

/* Pairing. addr is "AA:BB:CC:DD:EE:FF". 0 when the message went out. */
int btmtk_gap_bond(const char *addr);
int btmtk_gap_bond_cancel(const char *addr);
int btmtk_gap_user_confirm(const char *addr, int accept);
int btmtk_gap_pin_reply(const char *addr, const unsigned char *pin, int len);

/* Local name (up to 59 bytes) and scan mode. 0 when mtkbt confirmed. */
#define BTMTK_GAP_SCAN_INQUIRY 0x01    /* discoverable */
#define BTMTK_GAP_SCAN_PAGE    0x02    /* connectable */
int btmtk_gap_set_local_name(const char *name);
int btmtk_gap_set_scan_mode(int mode);
#define BTMTK_GAP_DEFAULT_COD  0x5a020c  /* phone, as BlueZ main.conf */
int btmtk_gap_set_local_cod(unsigned long cod);

enum {
    BTMTK_GAP_EV_NONE = 0,
    BTMTK_GAP_EV_DISCOVERY_STARTED,
    BTMTK_GAP_EV_DEVICE_FOUND,
    BTMTK_GAP_EV_DISCOVERY_FINISHED,
    BTMTK_GAP_EV_USER_CONFIRM,   /* addr, passkey */
    BTMTK_GAP_EV_PIN_REQUEST,    /* addr */
    BTMTK_GAP_EV_BOND_RESULT,    /* addr, result (0 = bonded) */
};

typedef struct {
    int type;
    char addr[18];      /* "AA:BB:CC:DD:EE:FF" */
    char name[249];     /* may be empty until the name update */
    unsigned int cod;
    int rssi;
    int passkey;
    int result;
} btmtk_gap_event_t;

/* mtkbt sends indications to the abstract socket bt.ext.adp; one reader.
 * event_wait returns 1 with *ev filled, 0 on timeout, -1 on error. */
int btmtk_gap_events_open(void);
void btmtk_gap_events_close(void);
int btmtk_gap_event_wait(btmtk_gap_event_t *ev, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
