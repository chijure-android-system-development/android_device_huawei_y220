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

enum {
    BTMTK_GAP_EV_NONE = 0,
    BTMTK_GAP_EV_DISCOVERY_STARTED,
    BTMTK_GAP_EV_DEVICE_FOUND,
    BTMTK_GAP_EV_DISCOVERY_FINISHED,
};

typedef struct {
    int type;
    char addr[18];      /* "AA:BB:CC:DD:EE:FF" */
    char name[249];     /* may be empty until the name update */
    unsigned int cod;
    int rssi;
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
