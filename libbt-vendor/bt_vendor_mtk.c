/*
 * Bluedroid vendor library for the MT6572 combo chip.
 *
 * The stock libbluetoothdrv.so (Y330-U01 4.2.2) powers the BT function of
 * WMT on, loads the chip patch through libbluetooth_mtk.so, sets the BD
 * address from NVRAM and returns an fd that carries H4 for commands,
 * events and ACL. The firmware is ready when that fd comes back, so FW_CFG
 * has nothing left to do.
 *
 * Based on the libbt-vendor-mtk of android_device_alps_m706 (CM11), with
 * the 4.4-only EPILOG op dropped for the 4.2 interface.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "bt_vendor_mtk"

#include <dlfcn.h>
#include <stdint.h>
#include <cutils/log.h>
#include <bt_vendor_lib.h>

/* Idle time before the stack lets the controller sleep (ms). */
#define MTK_LPM_IDLE_TIMEOUT 200

typedef int (*mtk_bt_enable_t)(int flag, void *func_cb);
typedef int (*mtk_bt_disable_t)(int bt_fd);

static void *drv;
static mtk_bt_enable_t mtk_bt_enable;
static mtk_bt_disable_t mtk_bt_disable;
static const bt_vendor_callbacks_t *cbacks;
static int bt_fd = -1;

static int mtk_init(const bt_vendor_callbacks_t *p_cb, unsigned char *local_bdaddr)
{
    if (p_cb == NULL) {
        ALOGE("init without callbacks");
        return -1;
    }
    cbacks = p_cb;

    drv = dlopen("libbluetoothdrv.so", RTLD_LAZY);
    if (drv == NULL) {
        ALOGE("cannot load libbluetoothdrv.so: %s", dlerror());
        return -1;
    }
    mtk_bt_enable = (mtk_bt_enable_t) dlsym(drv, "mtk_bt_enable");
    mtk_bt_disable = (mtk_bt_disable_t) dlsym(drv, "mtk_bt_disable");
    if (mtk_bt_enable == NULL || mtk_bt_disable == NULL) {
        ALOGE("libbluetoothdrv.so has no mtk_bt_enable/disable");
        dlclose(drv);
        drv = NULL;
        return -1;
    }
    return 0;
}

static int userial_open(void **param)
{
    int *fds = (int *) param;
    int ch;

    bt_fd = mtk_bt_enable(0, NULL);
    if (bt_fd < 0) {
        ALOGE("mtk_bt_enable failed: %d", bt_fd);
        return -1;
    }
    for (ch = 0; ch < CH_MAX; ch++)
        fds[ch] = bt_fd;
    /* One fd for CMD, EVT and ACL. */
    return 1;
}

static int userial_close(void)
{
    int ret;

    if (bt_fd < 0)
        return -1;
    ret = mtk_bt_disable(bt_fd);
    bt_fd = -1;
    return ret;
}

static int mtk_op(bt_vendor_opcode_t opcode, void *param)
{
    switch (opcode) {
    case BT_VND_OP_POWER_CTRL:
        /* mtk_bt_enable/disable power the chip with the port. */
        return 0;
    case BT_VND_OP_FW_CFG:
        cbacks->fwcfg_cb(BT_VND_OP_RESULT_SUCCESS);
        return 0;
    case BT_VND_OP_SCO_CFG:
        cbacks->scocfg_cb(BT_VND_OP_RESULT_SUCCESS);
        return 0;
    case BT_VND_OP_USERIAL_OPEN:
        return userial_open((void **) param);
    case BT_VND_OP_USERIAL_CLOSE:
        return userial_close();
    case BT_VND_OP_GET_LPM_IDLE_TIMEOUT:
        *(uint32_t *) param = MTK_LPM_IDLE_TIMEOUT;
        return 0;
    case BT_VND_OP_LPM_SET_MODE:
        cbacks->lpm_cb(BT_VND_OP_RESULT_SUCCESS);
        return 0;
    case BT_VND_OP_LPM_WAKE_SET_STATE:
        return 0;
    default:
        ALOGW("unknown op %d", opcode);
        return -1;
    }
}

static void mtk_cleanup(void)
{
    cbacks = NULL;
    if (drv != NULL) {
        dlclose(drv);
        drv = NULL;
    }
}

const bt_vendor_interface_t BLUETOOTH_VENDOR_LIB_INTERFACE = {
    sizeof(bt_vendor_interface_t),
    mtk_init,
    mtk_op,
    mtk_cleanup,
};
