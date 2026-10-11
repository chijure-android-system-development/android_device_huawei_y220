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

#ifdef __cplusplus
}
#endif

#endif
