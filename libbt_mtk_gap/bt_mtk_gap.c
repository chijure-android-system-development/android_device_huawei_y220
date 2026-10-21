#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>

#include <cutils/log.h>
#include <cutils/sockets.h>

#include "bt_mtk_gap.h"

#define BT_SOCK_NAME_INT_ADP "/dev/socket/bt.int.adp"
#define BT_SOCK_NAME_APP_GAP "bt.app.gap"
#define BT_SOCK_NAME_EXT_ADP "bt.ext.adp"
#define MAX_ILM_BUFFER_SIZE 2048
#define BTMTK_MAX_STACK_TIMEOUT 15000

#undef LOG_TAG
#define LOG_TAG "bt_mtk_gap"

typedef enum {
    MOD_BT = 0,
    MOD_TIMER,
    MOD_MMI,
    MOD_BWCS,
    MOD_TST,
    MOD_SPP_DEFAULT,
    MOD_PAL,
    END_OF_MOD_ID
} module_type;

typedef struct {
    unsigned char used;
    unsigned long msg_id;
    void *local_para_ptr;
    module_type src_mod_id;
    module_type dest_mod_id;
    unsigned char sap_id;
    void *peer_buff_ptr;
    unsigned char ilm_data[MAX_ILM_BUFFER_SIZE];
} ilm_struct;

typedef struct {
    unsigned char ref_count;
    unsigned short msg_len;
    unsigned char result;
} bt_cnf_struct;

/* Gap group starts at 100. The WISE-only ids are not in this daemon. */
enum {
    MSG_ID_BT_POWERON_REQ = 100,
    MSG_ID_BT_POWEROFF_REQ,
    MSG_ID_BT_RESET_REQ_IND,
    MSG_ID_BT_BM_DISCOVERY_REQ,
    MSG_ID_BT_BM_DISCOVERY_CANCEL_REQ,
    MSG_ID_BT_BM_WRITE_LOCAL_NAME_REQ,
    MSG_ID_BT_BM_READ_LOCAL_NAME_REQ,
    MSG_ID_BT_BM_READ_REMOTE_NAME_REQ,
    MSG_ID_BT_BM_READ_REMOTE_NAME_CANCEL_REQ,
    MSG_ID_BT_BM_WRITE_SCANENABLE_MODE_REQ,
    MSG_ID_BT_BM_READ_SCANENABLE_MODE_REQ,
    MSG_ID_BT_BM_READ_LOCAL_ADDR_REQ,
    MSG_ID_BT_BM_READ_LOCAL_COD_REQ,
    MSG_ID_BT_BM_WRITE_LOCAL_COD_REQ,
    MSG_ID_BT_BM_READ_LOCAL_UUID_REQ,
    MSG_ID_BT_BM_READ_PROPERTY_REQ,
    MSG_ID_BT_BM_WRITE_AUTHENTICATION_MODE_REQ,
    MSG_ID_BT_BM_PIN_CODE_RSP,
    MSG_ID_BT_BM_SECURITY_USER_CONFIRM_RSP,
    MSG_ID_BT_BM_SECURITY_PASSKEY_ENTRY_RSP,
    MSG_ID_BT_BM_SECURITY_PASSKEY_NOTIFY_RSP,
    MSG_ID_BT_BM_SECURITY_KEYPRESS_NOTIFY_RSP,
    MSG_ID_BT_BM_SECURITY_KEYPRESS_NOTIFY_CANCEL_REQ,
    MSG_ID_BT_BM_SECURITY_OOB_DATA_RSP,
    MSG_ID_BT_BM_BLOCK_ACTIVE_LINK_REQ,
    MSG_ID_BT_BM_BLOCK_LIST_UPDATE_REQ,
    MSG_ID_BT_BM_BONDING_REQ,
    MSG_ID_BT_BM_DELETE_TRUST_REQ,
    MSG_ID_BT_BM_DELETE_TRUST_ALL_REQ,
    MSG_ID_BT_BM_BONDING_CANCEL_REQ,
    MSG_ID_BT_BM_PAIRING_CANCEL_REQ,
    MSG_ID_BT_BM_SAVE_SETTING_REQ,
    MSG_ID_BT_BM_RESTORE_SETTING_REQ,
    MSG_ID_BT_BM_SERVICE_SEARCH_REQ,
    MSG_ID_BT_BM_SERVICE_SEARCH_CANCEL_REQ,
    MSG_ID_BT_BM_SEARCH_ATTRIBUTE_REQ,
    MSG_ID_BT_BM_SEARCH_RAW_REQ,
    MSG_ID_BT_BM_LINK_ALLOW_REQ,
    MSG_ID_BT_BM_LINK_DISALLOW_REQ,
    MSG_ID_BT_BM_LINK_CONNECT_ACCEPT_NOT_AUTO_REQ,
    MSG_ID_BT_BM_LINK_CONNECT_ACCEPT_RSP,
    MSG_ID_BT_BM_GET_LINK_STATE_REQ,
    MSG_ID_BT_BM_SET_LINK_STATE_REQ,
    MSG_ID_BT_BM_GET_SCATTERNET_STATE_REQ,
    MSG_ID_BT_READ_RSSI_REQ,
    MSG_ID_BT_POWERON_CNF,
    MSG_ID_BT_POWEROFF_CNF
};

static int s_sock = -1;

static int gap_sock(void)
{
    if (s_sock >= 0)
        return s_sock;
    s_sock = socket_local_server(BT_SOCK_NAME_APP_GAP,
                                 ANDROID_SOCKET_NAMESPACE_ABSTRACT,
                                 SOCK_DGRAM);
    if (s_sock < 0)
        LOGE("bind %s: %s", BT_SOCK_NAME_APP_GAP, strerror(errno));
    return s_sock;
}

static int send_req(unsigned long msg_id)
{
    ilm_struct ilm;
    struct sockaddr_un dest;
    socklen_t destlen;
    unsigned short *field;
    int sock;
    int n;
    int len;

    sock = gap_sock();
    if (sock < 0)
        return -1;

    memset(&ilm, 0, sizeof(ilm));
    ilm.msg_id = msg_id;
    ilm.src_mod_id = MOD_MMI;
    ilm.dest_mod_id = MOD_BT;
    field = (unsigned short *)ilm.ilm_data;
    *field = 1;
    *(++field) = 0;

    memset(&dest, 0, sizeof(dest));
    dest.sun_family = AF_UNIX;
    strcpy(dest.sun_path, BT_SOCK_NAME_INT_ADP);
    destlen = offsetof(struct sockaddr_un, sun_path) + strlen(dest.sun_path) + 1;
    len = sizeof(ilm_struct) - MAX_ILM_BUFFER_SIZE;
    n = sendto(sock, &ilm, len, 0, (struct sockaddr *)&dest, destlen);
    if (n < 0) {
        LOGE("send %lu: %s", msg_id, strerror(errno));
        return -1;
    }
    LOGI("send %lu (%d bytes)", msg_id, n);
    return 0;
}

static int wait_cnf(unsigned long msg_id, unsigned char *result, int timeout_ms)
{
    ilm_struct ilm;
    struct timeval start;
    struct timeval now;
    struct timeval left;
    fd_set readfs;
    int sock;
    int waited = 0;

    sock = gap_sock();
    if (sock < 0)
        return -1;
    if (gettimeofday(&start, NULL) != 0)
        return -1;

    while (waited < timeout_ms) {
        int slice = timeout_ms - waited;
        int n;

        left.tv_sec = slice / 1000;
        left.tv_usec = (slice % 1000) * 1000;
        FD_ZERO(&readfs);
        FD_SET(sock, &readfs);
        n = select(sock + 1, &readfs, NULL, NULL, &left);
        if (n == 0) {
            LOGE("timeout waiting %lu", msg_id);
            return -1;
        }
        if (n < 0) {
            if (errno == EINTR)
                continue;
            LOGE("select: %s", strerror(errno));
            return -1;
        }
        memset(&ilm, 0, sizeof(ilm));
        n = recvfrom(sock, &ilm, sizeof(ilm), 0, NULL, NULL);
        if (n < 0) {
            LOGE("recv: %s", strerror(errno));
            return -1;
        }
        if (ilm.msg_id != msg_id) {
            LOGW("skip msg %lu, want %lu (%d bytes)", ilm.msg_id, msg_id, n);
        } else {
            bt_cnf_struct *cnf = (bt_cnf_struct *)ilm.ilm_data;
            *result = cnf->result;
            LOGI("cnf %lu result=%u", msg_id, cnf->result);
            return (*result == 0 || *result == 1) ? 0 : -1;
        }
        if (gettimeofday(&now, NULL) != 0)
            return -1;
        waited = (int)((now.tv_sec - start.tv_sec) * 1000 +
                       (now.tv_usec - start.tv_usec) / 1000);
    }
    LOGE("timeout waiting %lu", msg_id);
    return -1;
}

static int gap_request(unsigned long req, unsigned long cnf)
{
    unsigned char result = 0xff;

    if (send_req(req) != 0)
        return -1;
    return wait_cnf(cnf, &result, BTMTK_MAX_STACK_TIMEOUT);
}

int btmtk_gap_power_on(void)
{
    LOGI("POWERON_REQ");
    if (gap_request(MSG_ID_BT_POWERON_REQ, MSG_ID_BT_POWERON_CNF) != 0)
        return -1;
    /* Stock sets the class from BluetoothService ("Class"); CM7 never
     * does, so write the BlueZ main.conf default here. */
    if (btmtk_gap_set_local_cod(BTMTK_GAP_DEFAULT_COD) != 0)
        LOGW("WRITE_LOCAL_COD failed, class left as mtkbt default");
    return 0;
}

int btmtk_gap_power_off(void)
{
    LOGI("POWEROFF_REQ");
    return gap_request(MSG_ID_BT_POWEROFF_REQ, MSG_ID_BT_POWEROFF_CNF);
}

int btmtk_gap_local_address(char *out, int outlen)
{
    unsigned char addr[6];
    int fd;
    int n;

    if (out == NULL || outlen < 18)
        return -1;
    fd = open("/data/BT_Addr", O_RDONLY);
    if (fd < 0)
        return -1;
    n = read(fd, addr, sizeof(addr));
    close(fd);
    if (n != (int)sizeof(addr))
        return -1;
    snprintf(out, outlen, "%02X:%02X:%02X:%02X:%02X:%02X",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    return 0;
}

/*
 * Discovery. The request goes to bt.int.adp like the others; mtkbt answers
 * on bt.ext.adp, not on bt.app.gap. Ids seen from this mtkbt: 147 result,
 * 148 name update (same layout), 149 discovery done (also after a cancel).
 */
#define MSG_ID_BT_BM_DISCOVERY_RESULT_IND 147
#define MSG_ID_BT_BM_DISCOVERY_UPDATE_IND 148
#define MSG_ID_BT_BM_DISCOVERY_CNF        149
/* Pairing, same source: 164 seen, 171/172 seen, 163 from the MTK order. */
#define MSG_ID_BT_BM_PIN_CODE_IND               163
#define MSG_ID_BT_BM_SECURITY_USER_CONFIRM_IND  164
#define MSG_ID_BT_BM_BONDING_RESULT_IND         172
#define MSG_ID_BT_BM_LINK_STATE_IND             186
#define MSG_ID_BT_BM_REMOTE_NAME_IND            194

static int s_ext = -1;
static int s_pipe[2] = { -1, -1 };
static volatile int s_discovering;

static int send_data(unsigned long msg_id, const void *data, int datalen)
{
    ilm_struct ilm;
    struct sockaddr_un dest;
    socklen_t destlen;
    int sock;
    int n;

    sock = gap_sock();
    if (sock < 0)
        return -1;

    memset(&ilm, 0, sizeof(ilm));
    ilm.msg_id = msg_id;
    ilm.src_mod_id = MOD_MMI;
    ilm.dest_mod_id = MOD_BT;
    memcpy(ilm.ilm_data, data, datalen);

    memset(&dest, 0, sizeof(dest));
    dest.sun_family = AF_UNIX;
    strcpy(dest.sun_path, BT_SOCK_NAME_INT_ADP);
    destlen = offsetof(struct sockaddr_un, sun_path) + strlen(dest.sun_path) + 1;

    n = sendto(sock, &ilm, offsetof(ilm_struct, ilm_data) + datalen, 0,
               (struct sockaddr *)&dest, destlen);
    if (n < 0) {
        LOGE("send %lu: %s", msg_id, strerror(errno));
        return -1;
    }
    LOGI("send %lu (%d bytes)", msg_id, n);
    return 0;
}

static void notify(char c)
{
    if (s_pipe[1] >= 0)
        write(s_pipe[1], &c, 1);
}

int btmtk_gap_discovery_start(void)
{
    /* bt_bm_discovery_req_struct: hdr, cod, inquiry_length, inquiry_number,
     * access_mode, discovery_mode (0 = with name), discovery_type. */
    unsigned char req[24];

    memset(req, 0, sizeof(req));
    req[0] = 1;                 /* ref_count */
    req[2] = sizeof(req);       /* msg_len */
    req[4] = req[5] = req[6] = req[7] = 0xff;   /* any class */
    req[8] = 0x08;              /* inquiry length, x1.28 s (stock 0x30) */
    req[9] = 25;                /* max results */

    LOGI("DISCOVERY_REQ");
    if (send_data(MSG_ID_BT_BM_DISCOVERY_REQ, req, sizeof(req)) != 0)
        return -1;
    s_discovering = 1;
    notify('S');
    return 0;
}

int btmtk_gap_discovery_cancel(void)
{
    unsigned char hdr[4] = { 1, 0, 4, 0 };

    if (!s_discovering)
        return 0;
    LOGI("DISCOVERY_CANCEL_REQ");
    s_discovering = 0;
    notify('F');
    return send_data(MSG_ID_BT_BM_DISCOVERY_CANCEL_REQ, hdr, sizeof(hdr));
}

int btmtk_gap_events_open(void)
{
    if (s_ext >= 0)
        return 0;
    s_ext = socket_local_server(BT_SOCK_NAME_EXT_ADP,
                                ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_DGRAM);
    if (s_ext < 0) {
        LOGE("bind %s: %s", BT_SOCK_NAME_EXT_ADP, strerror(errno));
        return -1;
    }
    if (pipe(s_pipe) != 0) {
        LOGE("pipe: %s", strerror(errno));
        close(s_ext);
        s_ext = -1;
        return -1;
    }
    return 0;
}

void btmtk_gap_events_close(void)
{
    if (s_ext >= 0)
        close(s_ext);
    if (s_pipe[0] >= 0)
        close(s_pipe[0]);
    if (s_pipe[1] >= 0)
        close(s_pipe[1]);
    s_ext = s_pipe[0] = s_pipe[1] = -1;
    s_discovering = 0;
}

/* bt_addr_struct: lap (3 of 4 bytes), uap, pad, nap. Padding is not zeroed
 * by mtkbt, so compare and build fields, never the 8 raw bytes. */
static int addr_pack(const char *s, unsigned char *out)
{
    unsigned int b[6];

    if (s == NULL || sscanf(s, "%x:%x:%x:%x:%x:%x",
                            &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6)
        return -1;
    memset(out, 0, 8);
    out[0] = b[5];
    out[1] = b[4];
    out[2] = b[3];
    out[4] = b[2];
    out[6] = b[1];
    out[7] = b[0];
    return 0;
}

static void addr_unpack(const unsigned char *p, char *out, int outlen)
{
    snprintf(out, outlen, "%02X:%02X:%02X:%02X:%02X:%02X",
             p[7], p[6], p[4], p[2], p[1], p[0]);
}

int btmtk_gap_bond(const char *addr)
{
    /* bt_bm_bonding_req_struct: hdr, bd_addr, sdap_len 0, sdap uuids. */
    unsigned char req[144];

    memset(req, 0, sizeof(req));
    req[0] = 1;
    req[2] = sizeof(req);
    if (addr_pack(addr, req + 4) != 0)
        return -1;
    LOGI("BONDING_REQ %s", addr);
    return send_data(MSG_ID_BT_BM_BONDING_REQ, req, sizeof(req));
}

int btmtk_gap_bond_cancel(const char *addr)
{
    unsigned char req[12];

    memset(req, 0, sizeof(req));
    req[0] = 1;
    req[2] = sizeof(req);
    if (addr_pack(addr, req + 4) != 0)
        return -1;
    LOGI("BONDING_CANCEL_REQ %s", addr);
    return send_data(MSG_ID_BT_BM_BONDING_CANCEL_REQ, req, sizeof(req));
}

int btmtk_gap_user_confirm(const char *addr, int accept)
{
    /* bt_bm_security_user_confirm_rsp_struct: hdr, bd_addr, accept. */
    unsigned char rsp[16];

    memset(rsp, 0, sizeof(rsp));
    rsp[0] = 1;
    rsp[2] = sizeof(rsp);
    if (addr_pack(addr, rsp + 4) != 0)
        return -1;
    rsp[12] = accept ? 1 : 0;
    LOGI("USER_CONFIRM_RSP %s %d", addr, accept);
    return send_data(MSG_ID_BT_BM_SECURITY_USER_CONFIRM_RSP, rsp, sizeof(rsp));
}

int btmtk_gap_pin_reply(const char *addr, const unsigned char *pin, int len)
{
    /* bt_bm_pin_code_rsp_struct: hdr, bd_addr, pin_len, pin_code[16].
     * pin_len 0 rejects. */
    unsigned char rsp[32];

    if (len < 0 || len > 16)
        return -1;
    memset(rsp, 0, sizeof(rsp));
    rsp[0] = 1;
    rsp[2] = sizeof(rsp);
    if (addr_pack(addr, rsp + 4) != 0)
        return -1;
    rsp[12] = len;
    if (len)
        memcpy(rsp + 13, pin, len);
    LOGI("PIN_CODE_RSP %s len %d", addr, len);
    return send_data(MSG_ID_BT_BM_PIN_CODE_RSP, rsp, sizeof(rsp));
}

/*
 * Local name and scan mode, laid out as the stock libandroid_runtime
 * btmtk_gap_set_local_name / btmtk_gap_set_scanable_mode build them.
 * Their CNFs (151, 155) carry the result at ilm_data[4], 0 = done.
 */
#define MSG_ID_BT_BM_WRITE_LOCAL_NAME_CNF     151
#define MSG_ID_BT_BM_WRITE_SCANENABLE_MODE_CNF 155
#define MSG_ID_BT_BM_WRITE_LOCAL_COD_CNF       159
#define BTMTK_GAP_SET_TIMEOUT 3000

static int write_req(unsigned long req_id, unsigned long cnf_id,
                     unsigned char *req, int len)
{
    unsigned char result = 0xff;

    req[0] = 1;
    req[2] = len & 0xff;
    req[3] = len >> 8;
    if (send_data(req_id, req, len) != 0)
        return -1;
    if (wait_cnf(cnf_id, &result, BTMTK_GAP_SET_TIMEOUT) != 0)
        return -1;
    return result == 0 ? 0 : -1;
}

int btmtk_gap_set_local_name(const char *name)
{
    /* bt_bm_write_local_name_req_struct: hdr, name_len, name. Stock caps
     * the length at 59 bytes. */
    unsigned char req[254];
    int len;

    if (name == NULL)
        return -1;
    len = strlen(name);
    if (len > 59)
        len = 59;
    memset(req, 0, sizeof(req));
    req[4] = len;
    memcpy(req + 5, name, len);
    LOGI("WRITE_LOCAL_NAME_REQ \"%.*s\"", len, name);
    return write_req(MSG_ID_BT_BM_WRITE_LOCAL_NAME_REQ,
                     MSG_ID_BT_BM_WRITE_LOCAL_NAME_CNF, req, sizeof(req));
}

int btmtk_gap_set_scan_mode(int mode)
{
    /* bt_bm_write_scanenable_mode_req_struct: hdr, mode. */
    unsigned char req[8];

    memset(req, 0, sizeof(req));
    req[4] = mode & (BTMTK_GAP_SCAN_INQUIRY | BTMTK_GAP_SCAN_PAGE);
    LOGI("WRITE_SCANENABLE_MODE_REQ %d", req[4]);
    return write_req(MSG_ID_BT_BM_WRITE_SCANENABLE_MODE_REQ,
                     MSG_ID_BT_BM_WRITE_SCANENABLE_MODE_CNF, req, sizeof(req));
}

int btmtk_gap_set_local_cod(unsigned long cod)
{
    /* bt_bm_write_local_cod_req_struct: hdr, cod, write_type (stock 2). */
    unsigned char req[12];

    memset(req, 0, sizeof(req));
    req[4] = cod & 0xff;
    req[5] = (cod >> 8) & 0xff;
    req[6] = (cod >> 16) & 0xff;
    req[8] = 2;
    LOGI("WRITE_LOCAL_COD_REQ 0x%06lx", cod);
    return write_req(MSG_ID_BT_BM_WRITE_LOCAL_COD_REQ,
                     MSG_ID_BT_BM_WRITE_LOCAL_COD_CNF, req, sizeof(req));
}

/* bt_bm_discovery_result_ind_struct as this mtkbt lays it out. */
static void parse_result(const unsigned char *d, int len, btmtk_gap_event_t *ev)
{
    unsigned int lap, uap, nap;
    int name_len;

    lap = d[4] | (d[5] << 8) | (d[6] << 16);
    uap = d[8];
    nap = d[10] | (d[11] << 8);
    snprintf(ev->addr, sizeof(ev->addr), "%02X:%02X:%02X:%02X:%02X:%02X",
             nap >> 8, nap & 0xff, uap, lap >> 16, (lap >> 8) & 0xff, lap & 0xff);
    ev->rssi = (signed char)d[40];
    ev->cod = d[44] | (d[45] << 8) | (d[46] << 16) | (d[47] << 24);
    name_len = d[52];
    if (name_len > len - 53)
        name_len = len - 53;
    if (name_len > (int)sizeof(ev->name) - 1)
        name_len = sizeof(ev->name) - 1;
    if (name_len < 0)
        name_len = 0;
    memcpy(ev->name, d + 53, name_len);
    ev->name[name_len] = 0;
}

int btmtk_gap_event_wait(btmtk_gap_event_t *ev, int timeout_ms)
{
    ilm_struct ilm;
    struct timeval tv;
    fd_set fs;
    int maxfd;
    int n;

    if (s_ext < 0 || ev == NULL)
        return -1;
    memset(ev, 0, sizeof(*ev));

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    FD_ZERO(&fs);
    FD_SET(s_ext, &fs);
    FD_SET(s_pipe[0], &fs);
    maxfd = s_ext > s_pipe[0] ? s_ext : s_pipe[0];
    n = select(maxfd + 1, &fs, NULL, NULL, &tv);
    if (n < 0)
        return errno == EINTR ? 0 : -1;
    if (n == 0)
        return 0;

    if (FD_ISSET(s_pipe[0], &fs)) {
        char c;
        if (read(s_pipe[0], &c, 1) == 1) {
            ev->type = (c == 'S') ? BTMTK_GAP_EV_DISCOVERY_STARTED
                                  : BTMTK_GAP_EV_DISCOVERY_FINISHED;
            return 1;
        }
        return 0;
    }

    n = recvfrom(s_ext, &ilm, sizeof(ilm), 0, NULL, NULL);
    if (n < (int)offsetof(ilm_struct, ilm_data))
        return 0;
    n -= offsetof(ilm_struct, ilm_data);

    if (ilm.msg_id == MSG_ID_BT_BM_BONDING_RESULT_IND ||
            ilm.msg_id == MSG_ID_BT_BM_SECURITY_USER_CONFIRM_IND) {
        /* TEMP: dump to find the remote class of device. */
        char hex[3 * 40 + 1];
        int i, m = n < 40 ? n : 40;
        for (i = 0; i < m; i++)
            snprintf(hex + 3 * i, 4, "%02x ", ilm.ilm_data[i]);
        hex[3 * m] = 0;
        LOGI("msg %lu (%d): %s", ilm.msg_id, n, hex);
    }
    switch (ilm.msg_id) {
    case MSG_ID_BT_BM_DISCOVERY_RESULT_IND:
    case MSG_ID_BT_BM_DISCOVERY_UPDATE_IND:
        if (n < 53)
            return 0;
        parse_result(ilm.ilm_data, n, ev);
        ev->type = BTMTK_GAP_EV_DEVICE_FOUND;
        return 1;
    case MSG_ID_BT_BM_DISCOVERY_CNF:
        if (!s_discovering)
            return 0;
        s_discovering = 0;
        ev->type = BTMTK_GAP_EV_DISCOVERY_FINISHED;
        return 1;
    case MSG_ID_BT_BM_SECURITY_USER_CONFIRM_IND:
        /* hdr, bd_addr, numeric[6] ASCII, display_numeric, cod, name. */
        if (n < 18)
            return 0;
        addr_unpack(ilm.ilm_data + 4, ev->addr, sizeof(ev->addr));
        {
            char num[7];
            memcpy(num, ilm.ilm_data + 12, 6);
            num[6] = 0;
            ev->passkey = atoi(num);
        }
        ev->type = BTMTK_GAP_EV_USER_CONFIRM;
        return 1;
    case MSG_ID_BT_BM_PIN_CODE_IND:
        if (n < 12)
            return 0;
        addr_unpack(ilm.ilm_data + 4, ev->addr, sizeof(ev->addr));
        ev->type = BTMTK_GAP_EV_PIN_REQUEST;
        return 1;
    case MSG_ID_BT_BM_BONDING_RESULT_IND:
        /* hdr, result, pad, bd_addr at 8, cod, link key type, key. */
        if (n < 16)
            return 0;
        ev->result = ilm.ilm_data[4];
        addr_unpack(ilm.ilm_data + 8, ev->addr, sizeof(ev->addr));
        ev->type = BTMTK_GAP_EV_BOND_RESULT;
        return 1;
    case MSG_ID_BT_BM_REMOTE_NAME_IND:
        /* hdr, bd_addr, name_len, name. Sent after pairing with a device
         * that discovery never saw (incoming pairing). */
        if (n < 13 || ilm.ilm_data[12] == 0)
            return 0;
        addr_unpack(ilm.ilm_data + 4, ev->addr, sizeof(ev->addr));
        {
            int len = ilm.ilm_data[12];
            if (len > n - 13)
                len = n - 13;
            if (len > (int)sizeof(ev->name) - 1)
                len = sizeof(ev->name) - 1;
            memcpy(ev->name, ilm.ilm_data + 13, len);
            ev->name[len] = 0;
        }
        LOGI("REMOTE_NAME_IND %s \"%s\"", ev->addr, ev->name);
        ev->type = BTMTK_GAP_EV_REMOTE_NAME;
        return 1;
    case MSG_ID_BT_BM_LINK_STATE_IND:
        /* hdr, 0x04, state (ilm_data[5]: 1 up, 0 down), bd_addr at 8. */
        return 0;
    default:
        LOGI("ext msg %lu (%d bytes)", ilm.msg_id, n);
        return 0;
    }
}
