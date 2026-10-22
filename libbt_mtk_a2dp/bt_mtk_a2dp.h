/*
 * A2DP source control over mtkbt, through the stock libmtkbtextadpa2dp.
 * The audio itself goes through libmtka2dp in mediaserver (liba2dp API).
 */
#ifndef BT_MTK_A2DP_H
#define BT_MTK_A2DP_H

#ifdef __cplusplus
extern "C" {
#endif

/* BlueZ AudioSink states; libmtkbtextadpa2dp uses the same numbers. */
enum {
    BTMTK_A2DP_NONE = 0,
    BTMTK_A2DP_DISCONNECTED = 1,
    BTMTK_A2DP_CONNECTING = 2,
    BTMTK_A2DP_CONNECTED = 3,
    BTMTK_A2DP_PLAYING = 4,
};

const char *btmtk_a2dp_state_name(int state);

/* Bluetooth on: sockets and ACTIVATE_REQ. 0 on success. */
int btmtk_a2dp_start(void);
void btmtk_a2dp_stop(void);

/* Waits for an indication from mtkbt. Returns a state (1..4) with addr set
 * when the connection state changed, 0 on timeout or nothing to report,
 * -1 when not started. */
int btmtk_a2dp_event_wait(char addr[18], int timeout_ms);

int btmtk_a2dp_connect(const char *addr);
int btmtk_a2dp_disconnect(void);
int btmtk_a2dp_suspend(void);
int btmtk_a2dp_resume(void);
/* Current state and, when there is one, the remote address. */
int btmtk_a2dp_get_state(char addr[18]);

#ifdef __cplusplus
}
#endif

#endif
