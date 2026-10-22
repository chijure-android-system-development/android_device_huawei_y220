/*
 * OPP client and server for the CM7 Bluetooth app over mtkbt.
 *
 * BlueZ RFCOMM does not exist on this kernel; mtkbt does OBEX itself. This
 * drives the stock libextopp.so the way the stock libextopp_jni does
 * (enableServiceNative, startListenNative, oppcConnectNative, oppcPushNative):
 *  - an api socket connected to /dev/socket/bt.int.adp and a server socket
 *    on the abstract bt.ext.adp.opp go into g_bt_opp_cntx at 0x560/0x564;
 *  - startListenMessage() is libextopp's receive loop, run on its own thread;
 *  - every result comes back through the jni callback as (event, params[]),
 *    with the bt_opp_ui_event ids of bt_opp_comm.h: client events below
 *    100, server events from 101.
 *  - the server (stock oppsEnableNative/oppsAccessResponseNative) answers a
 *    push request with (0, {total bytes, full path}) and libextopp writes
 *    the file itself, in this process.
 * libextopp is dlopen'ed: it is a vendor blob and not in the build output.
 */

#define LOG_TAG "bt_mtk_opp"

#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cutils/log.h>
#include <cutils/sockets.h>
#include <jni.h>

#define OPP_CLASS "com/android/bluetooth/opp/BluetoothOppMtk"
#define OPP_CTX_APISOCK  0x560
#define OPP_CTX_SERVSOCK 0x564

struct bt_addr_struct {
    unsigned int lap;
    unsigned char uap;
    unsigned short nap;
};

typedef void (*opp_jni_cb_t)(unsigned char event, char **params, unsigned char count);

static struct {
    char **ctx;
    int (*set_jni_callback)(opp_jni_cb_t);
    void (*client_enable)(void);
    void (*client_connect)(bt_addr_struct *);
    void (*client_push)(const char *, const char *, const char *);
    void (*client_abort)(void);
    void (*client_disconnect)(void);
    void (*server_enable)(void);
    void (*server_disable)(void);
    void (*server_disconnect)(void);
    void (*server_access_response)(int, char **);
    void (*string2bdaddr)(const char *, bt_addr_struct *);
    void (*listen)(void);
} s_opp;

static JavaVM *s_vm;
static jclass s_class;
static jmethodID s_onEvent;
static int s_ready;
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;

static void opp_cb(unsigned char event, char **params, unsigned char count)
{
    JNIEnv *env;
    bool attached = false;
    jobjectArray arr = NULL;

    if (s_vm->GetEnv((void **)&env, JNI_VERSION_1_4) != JNI_OK) {
        if (s_vm->AttachCurrentThread(&env, NULL) != JNI_OK)
            return;
        attached = true;
    }
    if (params != NULL && count > 0) {
        jclass str = env->FindClass("java/lang/String");
        arr = env->NewObjectArray(count, str, NULL);
        for (int i = 0; i < count; i++) {
            jstring s = env->NewStringUTF(params[i] ? params[i] : "");
            env->SetObjectArrayElement(arr, i, s);
            env->DeleteLocalRef(s);
        }
        env->DeleteLocalRef(str);
    }
    env->CallStaticVoidMethod(s_class, s_onEvent, (jint)event, arr);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
    if (arr)
        env->DeleteLocalRef(arr);
    if (attached)
        s_vm->DetachCurrentThread();
}

static void *listen_thread(void *)
{
    s_opp.listen();
    LOGW("libextopp listen loop ended");
    return NULL;
}

static bool load_libextopp(void)
{
    void *h = dlopen("/system/lib/libextopp.so", RTLD_NOW);
    if (h == NULL) {
        LOGE("dlopen libextopp: %s", dlerror());
        return false;
    }
    s_opp.ctx = (char **)dlsym(h, "g_bt_opp_cntx_p");
    *(void **)&s_opp.set_jni_callback = dlsym(h, "btmtk_opp_set_jni_callback");
    *(void **)&s_opp.client_enable = dlsym(h, "btmtk_opp_client_enable");
    *(void **)&s_opp.client_connect = dlsym(h, "btmtk_opp_client_connect");
    *(void **)&s_opp.client_push = dlsym(h, "btmtk_opp_client_push");
    *(void **)&s_opp.client_abort = dlsym(h, "btmtk_opp_client_abort");
    *(void **)&s_opp.client_disconnect = dlsym(h, "btmtk_opp_client_disconnect");
    *(void **)&s_opp.server_enable = dlsym(h, "btmtk_opp_server_enable");
    *(void **)&s_opp.server_disable = dlsym(h, "btmtk_opp_server_disable");
    *(void **)&s_opp.server_disconnect = dlsym(h, "btmtk_opp_server_disconnect");
    *(void **)&s_opp.server_access_response = dlsym(h, "btmtk_opp_server_access_response");
    *(void **)&s_opp.string2bdaddr = dlsym(h, "btmtk_util_convert_string2bdaddr");
    *(void **)&s_opp.listen = dlsym(h, "startListenMessage");
    if (!s_opp.ctx || !s_opp.set_jni_callback || !s_opp.client_enable ||
            !s_opp.client_connect || !s_opp.client_push || !s_opp.client_abort ||
            !s_opp.client_disconnect || !s_opp.server_enable || !s_opp.server_disable ||
            !s_opp.server_disconnect || !s_opp.server_access_response ||
            !s_opp.string2bdaddr || !s_opp.listen) {
        LOGE("libextopp is missing a symbol");
        return false;
    }
    return true;
}

/* Stock enableServiceNative + startListenNative, once per process. */
static bool setup(void)
{
    struct sockaddr_un a;
    int api, serv;
    pthread_t t;

    pthread_mutex_lock(&s_lock);
    if (s_ready) {
        pthread_mutex_unlock(&s_lock);
        return true;
    }
    if (s_opp.listen == NULL && !load_libextopp())
        goto fail;

    api = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (api < 0)
        goto fail;
    memset(&a, 0, sizeof(a));
    a.sun_family = AF_UNIX;
    strcpy(a.sun_path, "/dev/socket/bt.int.adp");
    if (connect(api, (struct sockaddr *)&a, strlen(a.sun_path) + 3) < 0) {
        LOGE("connect bt.int.adp: %s", strerror(errno));
        close(api);
        goto fail;
    }
    serv = socket_local_server("bt.ext.adp.opp", ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_DGRAM);
    if (serv < 0) {
        LOGE("bind bt.ext.adp.opp: %s", strerror(errno));
        close(api);
        goto fail;
    }
    *(int *)(*s_opp.ctx + OPP_CTX_APISOCK) = api;
    *(int *)(*s_opp.ctx + OPP_CTX_SERVSOCK) = serv;
    s_opp.set_jni_callback(opp_cb);
    if (pthread_create(&t, NULL, listen_thread, NULL) != 0) {
        close(api);
        close(serv);
        goto fail;
    }
    pthread_detach(t);
    s_ready = 1;
    pthread_mutex_unlock(&s_lock);
    return true;
fail:
    pthread_mutex_unlock(&s_lock);
    return false;
}

/* oppcEnableNative. Each client session enables; a second btmtk_oppc_init
 * answers ENABLE_FAIL, which is fine. */
static jboolean enableNative(JNIEnv *, jclass)
{
    if (!setup())
        return JNI_FALSE;
    /* Answers synchronously with BT_OPPC_ENABLE_SUCCESS/FAIL. */
    s_opp.client_enable();
    return JNI_TRUE;
}

/* oppsEnableNative: mtkbt registers the OPP SDP record and accepts pushes.
 * Answers with BT_OPPS_ENABLE_SUCCESS/FAIL (101/102). */
static jboolean serverEnableNative(JNIEnv *, jclass)
{
    if (!setup())
        return JNI_FALSE;
    s_opp.server_enable();
    return JNI_TRUE;
}

static void serverDisableNative(JNIEnv *, jclass)
{
    if (s_ready)
        s_opp.server_disable();
}

static void serverDisconnectNative(JNIEnv *, jclass)
{
    if (s_ready)
        s_opp.server_disconnect();
}

/* oppsAccessResponseNative: code 0 accepts and saves to path; otherwise the
 * OBEX response without the final bit (0x43 forbidden), as stock. */
static void serverAccessResponseNative(JNIEnv *env, jclass, jint code, jstring size,
        jstring path)
{
    const char *sz, *p;
    char *params[2];

    if (!s_ready || size == NULL || path == NULL)
        return;
    sz = env->GetStringUTFChars(size, NULL);
    p = env->GetStringUTFChars(path, NULL);
    params[0] = (char *)sz;
    params[1] = (char *)p;
    s_opp.server_access_response(code, params);
    env->ReleaseStringUTFChars(path, p);
    env->ReleaseStringUTFChars(size, sz);
}

static jboolean connectNative(JNIEnv *env, jclass, jstring address)
{
    bt_addr_struct addr;
    const char *c;

    if (!s_ready || address == NULL)
        return JNI_FALSE;
    c = env->GetStringUTFChars(address, NULL);
    memset(&addr, 0, sizeof(addr));
    s_opp.string2bdaddr(c, &addr);
    env->ReleaseStringUTFChars(address, c);
    s_opp.client_connect(&addr);
    return JNI_TRUE;
}

static jboolean pushNative(JNIEnv *env, jclass, jstring mime, jstring name, jstring path)
{
    const char *m, *n, *p;

    if (!s_ready || mime == NULL || name == NULL || path == NULL)
        return JNI_FALSE;
    m = env->GetStringUTFChars(mime, NULL);
    n = env->GetStringUTFChars(name, NULL);
    p = env->GetStringUTFChars(path, NULL);
    s_opp.client_push(m, n, p);
    env->ReleaseStringUTFChars(path, p);
    env->ReleaseStringUTFChars(name, n);
    env->ReleaseStringUTFChars(mime, m);
    return JNI_TRUE;
}

static void abortNative(JNIEnv *, jclass)
{
    if (s_ready)
        s_opp.client_abort();
}

static void disconnectNative(JNIEnv *, jclass)
{
    if (s_ready)
        s_opp.client_disconnect();
}

static JNINativeMethod sMethods[] = {
    {"enableNative", "()Z", (void *)enableNative},
    {"connectNative", "(Ljava/lang/String;)Z", (void *)connectNative},
    {"pushNative", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z", (void *)pushNative},
    {"abortNative", "()V", (void *)abortNative},
    {"disconnectNative", "()V", (void *)disconnectNative},
    {"serverEnableNative", "()Z", (void *)serverEnableNative},
    {"serverDisableNative", "()V", (void *)serverDisableNative},
    {"serverDisconnectNative", "()V", (void *)serverDisconnectNative},
    {"serverAccessResponseNative", "(ILjava/lang/String;Ljava/lang/String;)V",
            (void *)serverAccessResponseNative},
};

jint JNI_OnLoad(JavaVM *vm, void *)
{
    JNIEnv *env;
    jclass c;

    if (vm->GetEnv((void **)&env, JNI_VERSION_1_4) != JNI_OK)
        return -1;
    c = env->FindClass(OPP_CLASS);
    if (c == NULL)
        return -1;
    if (env->RegisterNatives(c, sMethods, sizeof(sMethods) / sizeof(sMethods[0])) < 0)
        return -1;
    s_onEvent = env->GetStaticMethodID(c, "onEvent", "(I[Ljava/lang/String;)V");
    if (s_onEvent == NULL)
        return -1;
    s_class = (jclass)env->NewGlobalRef(c);
    s_vm = vm;
    return JNI_VERSION_1_4;
}
