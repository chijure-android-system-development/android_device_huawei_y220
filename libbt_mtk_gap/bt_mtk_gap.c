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

static int wait_cnf(unsigned long msg_id, unsigned char *result)
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

    while (waited < BTMTK_MAX_STACK_TIMEOUT) {
        int slice = BTMTK_MAX_STACK_TIMEOUT - waited;
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
    return wait_cnf(cnf, &result);
}

int btmtk_gap_power_on(void)
{
    LOGI("POWERON_REQ");
    return gap_request(MSG_ID_BT_POWERON_REQ, MSG_ID_BT_POWERON_CNF);
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
    req[8] = 0x30;              /* inquiry length, x1.28 s */
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
    default:
        LOGI("ext msg %lu (%d bytes)", ilm.msg_id, n);
        return 0;
    }
}
