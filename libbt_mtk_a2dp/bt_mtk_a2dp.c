/*
 * A2DP source control over mtkbt.
 *
 * This repeats the A2DP part of the stock libandroid_runtime
 * (android_server_BluetoothA2dpService.cpp of the MTK framework, read with
 * Ghidra): the stock libmtkbtextadpa2dp.so keeps the whole AVDTP state
 * machine and talks to mtkbt with ilm messages; the caller only owns two
 * sockets and feeds it what arrives.
 *  - a server socket on the abstract bt.ext.adp.a2dp, whose address goes to
 *    btmtk_a2dp_setSockAddress();
 *  - an api socket connected to /dev/socket/bt.int.adp;
 *  - btmtk_a2dp_send_activate_req(server, api) when Bluetooth turns on;
 *  - every datagram on the server socket goes to btmtk_a2dp_handle_message(),
 *    which returns 1..4 (disconnected, connecting, connected, playing) when
 *    the connection state changed; btmtk_a2dp_get_bdaddr() names the device.
 * libmtkbtextadpa2dp is dlopen'ed: it is a vendor blob, not a build output.
 */

#define LOG_TAG "bt_mtk_a2dp"

#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include <cutils/log.h>
#include <cutils/sockets.h>

#include "bt_mtk_a2dp.h"

#define A2DP_SERVER_NAME "bt.ext.adp.a2dp"
#define BT_INT_ADP       "/dev/socket/bt.int.adp"
/* Stock receive buffer: ilm header plus the largest a2dp message. */
#define A2DP_MSG_MAX     0x81c

typedef struct {
    unsigned int lap;
    unsigned char uap;
    unsigned short nap;
} bt_addr_struct;

static struct {
    void (*set_sock_address)(void *, int);
    void (*activate_req)(int, int);
    int (*handle_message)(void *);
    void (*get_bdaddr)(bt_addr_struct *);
    int (*get_ap_state)(void);
    void (*connect_req)(bt_addr_struct *, unsigned char);
    void (*close_disconnect)(void);
    void (*start_request)(void);
    void (*stop_request)(void);
} s_lib;

static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static int s_srv = -1;
static int s_api = -1;

const char *btmtk_a2dp_state_name(int state)
{
    switch (state) {
    case BTMTK_A2DP_DISCONNECTED: return "disconnected";
    case BTMTK_A2DP_CONNECTING: return "connecting";
    case BTMTK_A2DP_CONNECTED: return "connected";
    case BTMTK_A2DP_PLAYING: return "playing";
    }
    return NULL;
}

static int load_lib(void)
{
    void *h;

    if (s_lib.handle_message != NULL)
        return 0;
    h = dlopen("/system/lib/libmtkbtextadpa2dp.so", RTLD_NOW);
    if (h == NULL) {
        LOGE("dlopen libmtkbtextadpa2dp: %s", dlerror());
        return -1;
    }
    *(void **)&s_lib.set_sock_address = dlsym(h, "btmtk_a2dp_setSockAddress");
    *(void **)&s_lib.activate_req = dlsym(h, "btmtk_a2dp_send_activate_req");
    *(void **)&s_lib.get_bdaddr = dlsym(h, "btmtk_a2dp_get_bdaddr");
    *(void **)&s_lib.get_ap_state = dlsym(h, "btmtk_a2dp_get_ap_state");
    *(void **)&s_lib.connect_req = dlsym(h, "btmtk_a2dp_send_signal_connect_req");
    *(void **)&s_lib.close_disconnect = dlsym(h, "a2dp_close_disconnect");
    *(void **)&s_lib.start_request = dlsym(h, "btmtk_A2dp_send_appi_bt_start_request");
    *(void **)&s_lib.stop_request = dlsym(h, "btmtk_A2dp_send_appi_bt_stop_request");
    *(void **)&s_lib.handle_message = dlsym(h, "btmtk_a2dp_handle_message");
    if (!s_lib.set_sock_address || !s_lib.activate_req || !s_lib.get_bdaddr ||
            !s_lib.get_ap_state || !s_lib.connect_req || !s_lib.close_disconnect ||
            !s_lib.start_request || !s_lib.stop_request || !s_lib.handle_message) {
        LOGE("libmtkbtextadpa2dp is missing a symbol");
        s_lib.handle_message = NULL;
        return -1;
    }
    return 0;
}

static void addr_pack(const char *s, bt_addr_struct *a)
{
    unsigned int b[6] = {0, 0, 0, 0, 0, 0};

    sscanf(s, "%02x:%02x:%02x:%02x:%02x:%02x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
    memset(a, 0, sizeof(*a));
    a->nap = (b[0] << 8) | b[1];
    a->uap = b[2];
    a->lap = (b[3] << 16) | (b[4] << 8) | b[5];
}

static void addr_unpack(const bt_addr_struct *a, char out[18])
{
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             (a->nap >> 8) & 0xff, a->nap & 0xff, a->uap,
             (a->lap >> 16) & 0xff, (a->lap >> 8) & 0xff, a->lap & 0xff);
}

int btmtk_a2dp_start(void)
{
    struct sockaddr_un srvaddr, adp;
    socklen_t len;
    struct timeval tv = {3, 0};

    pthread_mutex_lock(&s_lock);
    if (s_srv >= 0) {
        pthread_mutex_unlock(&s_lock);
        return 0;
    }
    if (load_lib() != 0)
        goto fail;

    s_srv = socket_local_server(A2DP_SERVER_NAME, ANDROID_SOCKET_NAMESPACE_ABSTRACT,
                                SOCK_DGRAM);
    if (s_srv < 0) {
        LOGE("server socket %s: %s", A2DP_SERVER_NAME, strerror(errno));
        goto fail;
    }
    memset(&srvaddr, 0, sizeof(srvaddr));
    len = sizeof(srvaddr);
    if (getsockname(s_srv, (struct sockaddr *)&srvaddr, &len) < 0) {
        LOGE("getsockname: %s", strerror(errno));
        goto fail;
    }
    s_lib.set_sock_address(&srvaddr, len);

    s_api = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (s_api < 0) {
        LOGE("api socket: %s", strerror(errno));
        goto fail;
    }
    setsockopt(s_api, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    /* Address length 2 (family only) autobinds an abstract name. */
    if (bind(s_api, (struct sockaddr *)&srvaddr, sizeof(sa_family_t)) < 0) {
        LOGE("bind api socket: %s", strerror(errno));
        goto fail;
    }
    memset(&adp, 0, sizeof(adp));
    adp.sun_family = AF_UNIX;
    strcpy(adp.sun_path, BT_INT_ADP);
    if (connect(s_api, (struct sockaddr *)&adp, strlen(adp.sun_path) + 3) < 0) {
        LOGE("connect %s: %s", BT_INT_ADP, strerror(errno));
        goto fail;
    }
    s_lib.activate_req(s_srv, s_api);
    LOGI("A2DP activate sent (server %d, api %d)", s_srv, s_api);
    pthread_mutex_unlock(&s_lock);
    return 0;

fail:
    if (s_srv >= 0)
        close(s_srv);
    if (s_api >= 0)
        close(s_api);
    s_srv = s_api = -1;
    pthread_mutex_unlock(&s_lock);
    return -1;
}

void btmtk_a2dp_stop(void)
{
    pthread_mutex_lock(&s_lock);
    if (s_srv >= 0)
        close(s_srv);
    if (s_api >= 0)
        close(s_api);
    s_srv = s_api = -1;
    pthread_mutex_unlock(&s_lock);
}

int btmtk_a2dp_event_wait(char addr[18], int timeout_ms)
{
    unsigned char buf[A2DP_MSG_MAX];
    struct timeval tv;
    fd_set fs;
    int fd, n, state;

    pthread_mutex_lock(&s_lock);
    fd = s_srv;
    pthread_mutex_unlock(&s_lock);
    if (fd < 0)
        return -1;

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    FD_ZERO(&fs);
    FD_SET(fd, &fs);
    if (select(fd + 1, &fs, NULL, NULL, &tv) <= 0)
        return 0;
    n = recvfrom(fd, buf, sizeof(buf), 0, NULL, NULL);
    /* Stock skips 4-byte datagrams. */
    if (n <= 4)
        return 0;

    pthread_mutex_lock(&s_lock);
    state = s_lib.handle_message(buf);
    if (state >= BTMTK_A2DP_DISCONNECTED && state <= BTMTK_A2DP_PLAYING) {
        bt_addr_struct a;
        memset(&a, 0, sizeof(a));
        s_lib.get_bdaddr(&a);
        addr_unpack(&a, addr);
        LOGI("state %s %s", btmtk_a2dp_state_name(state), addr);
    } else {
        state = BTMTK_A2DP_NONE;
    }
    pthread_mutex_unlock(&s_lock);
    return state;
}

int btmtk_a2dp_connect(const char *addr)
{
    bt_addr_struct a;

    if (btmtk_a2dp_start() != 0)
        return -1;
    addr_pack(addr, &a);
    LOGI("connect %s", addr);
    pthread_mutex_lock(&s_lock);
    /* Stock passes 0 as the second argument. */
    s_lib.connect_req(&a, 0);
    pthread_mutex_unlock(&s_lock);
    return 0;
}

static int call(void (*fn)(void), const char *what)
{
    if (btmtk_a2dp_start() != 0)
        return -1;
    LOGI("%s", what);
    pthread_mutex_lock(&s_lock);
    fn();
    pthread_mutex_unlock(&s_lock);
    return 0;
}

int btmtk_a2dp_disconnect(void)
{
    return s_lib.close_disconnect ? call(s_lib.close_disconnect, "disconnect") : -1;
}

int btmtk_a2dp_suspend(void)
{
    return s_lib.stop_request ? call(s_lib.stop_request, "suspend") : -1;
}

int btmtk_a2dp_resume(void)
{
    return s_lib.start_request ? call(s_lib.start_request, "resume") : -1;
}

int btmtk_a2dp_get_state(char addr[18])
{
    int state;
    bt_addr_struct a;

    if (load_lib() != 0)
        return BTMTK_A2DP_DISCONNECTED;
    pthread_mutex_lock(&s_lock);
    state = s_lib.get_ap_state();
    memset(&a, 0, sizeof(a));
    s_lib.get_bdaddr(&a);
    pthread_mutex_unlock(&s_lock);
    addr_unpack(&a, addr);
    if (state < BTMTK_A2DP_DISCONNECTED || state > BTMTK_A2DP_PLAYING)
        state = BTMTK_A2DP_DISCONNECTED;
    return state;
}
