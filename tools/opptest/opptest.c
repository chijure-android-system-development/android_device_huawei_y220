/* opptest: push one file over mtkbt OPP with the stock libextopp, the same
 * way libbt_mtk_opp does, without the Bluetooth app. Prints every libextopp
 * event (bt_opp_ui_event ids). Run as root with BT on:
 *   opptest 00:A0:C6:F2:E5:FB image/jpeg photo.jpg /mnt/sdcard/photo.jpg */
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cutils/sockets.h>

typedef struct { unsigned int lap; unsigned char uap; unsigned short nap; } bt_addr_struct;
typedef void (*jni_cb_t)(unsigned char event, char **params, unsigned char count);

static volatile int s_last = -1;

static void cb(unsigned char event, char **params, unsigned char count)
{
    int i;
    printf("event %u:", event);
    for (i = 0; i < count; i++)
        printf(" [%s]", params[i] ? params[i] : "(null)");
    printf("\n");
    fflush(stdout);
    s_last = event;
}

static void (*p_listen)(void);
static void *listen_thread(void *arg) { (void)arg; p_listen(); return NULL; }

static int wait_event(int want, int fail, int secs)
{
    int i;
    for (i = 0; i < secs * 10; i++) {
        if (s_last == want) return 0;
        if (s_last == fail) return -1;
        usleep(100000);
    }
    return -2;
}

int main(int argc, char **argv)
{
    void *h;
    char **ctx;
    int api, serv;
    struct sockaddr_un a;
    bt_addr_struct addr;
    pthread_t t;

    if (argc < 5) {
        fprintf(stderr, "usage: opptest <bdaddr> <mime> <name> <path>\n");
        return 1;
    }
    h = dlopen("/system/lib/libextopp.so", RTLD_NOW);
    if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    ctx = (char **)dlsym(h, "g_bt_opp_cntx_p");
    int (*set_cb)(jni_cb_t) = dlsym(h, "btmtk_opp_set_jni_callback");
    void (*enable)(void) = dlsym(h, "btmtk_opp_client_enable");
    void (*conn)(bt_addr_struct *) = dlsym(h, "btmtk_opp_client_connect");
    void (*push)(const char *, const char *, const char *) = dlsym(h, "btmtk_opp_client_push");
    void (*disc)(void) = dlsym(h, "btmtk_opp_client_disconnect");
    void (*s2a)(const char *, bt_addr_struct *) = dlsym(h, "btmtk_util_convert_string2bdaddr");
    p_listen = dlsym(h, "startListenMessage");
    if (!ctx || !set_cb || !enable || !conn || !push || !disc || !s2a || !p_listen) {
        fprintf(stderr, "dlsym failed\n");
        return 1;
    }

    api = socket(AF_UNIX, SOCK_DGRAM, 0);
    memset(&a, 0, sizeof(a));
    a.sun_family = AF_UNIX;
    strcpy(a.sun_path, "/dev/socket/bt.int.adp");
    if (connect(api, (struct sockaddr *)&a, strlen(a.sun_path) + 3) < 0) { perror("connect"); return 1; }
    serv = socket_local_server("bt.ext.adp.opp", ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_DGRAM);
    if (serv < 0) { perror("server"); return 1; }
    *(int *)(*ctx + 0x560) = api;
    *(int *)(*ctx + 0x564) = serv;
    set_cb(cb);
    pthread_create(&t, NULL, listen_thread, NULL);

    enable();
    if (wait_event(1, 2, 3)) { printf("enable failed\n"); return 1; }
    memset(&addr, 0, sizeof(addr));
    s2a(argv[1], &addr);
    printf("connect lap=%06x uap=%02x nap=%04x\n", addr.lap, addr.uap, addr.nap);
    conn(&addr);
    if (wait_event(5, 16, 30)) { printf("connect failed (%d)\n", s_last); return 1; }
    push(argv[2], argv[3], argv[4]);
    if (wait_event(8, 9, 120)) printf("push failed (%d)\n", s_last);
    else printf("push ok\n");
    disc();
    wait_event(16, -1, 10);
    return 0;
}
