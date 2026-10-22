/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>


#include <netinet/in.h>
#include <arpa/inet.h>

#define LOG_TAG "UsbController"
#include <cutils/log.h>

#include "UsbController.h"

/*
 * Kernels with the android_usb gadget (3.x) have no usb_composite/rndis:
 * RNDIS is a function name in android0/functions, and rndis0 only exists
 * while it is bound. Switch functions there and put the old list back.
 */
#define ANDROID_USB "/sys/class/android_usb/android0/"

static char sSavedFunctions[256];

static int readSys(const char *path, char *buf, int len) {
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;
    int n = read(fd, buf, len - 1);
    close(fd);
    if (n < 0)
        return -1;
    buf[n] = '\0';
    char *nl = strchr(buf, '\n');
    if (nl)
        *nl = '\0';
    return 0;
}

static int writeSys(const char *path, const char *value) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        LOGE("Unable to open %s (%s)", path, strerror(errno));
        return -1;
    }
    int n = write(fd, value, strlen(value));
    close(fd);
    if (n < 0) {
        LOGE("Unable to write %s to %s (%s)", value, path, strerror(errno));
        return -1;
    }
    return 0;
}

static bool hasAndroidUsb() {
    return access(ANDROID_USB "functions", W_OK) == 0;
}

static int enableAndroidUsbRNDIS(bool enable) {
    char cur[256], next[256];

    if (readSys(ANDROID_USB "functions", cur, sizeof(cur)) < 0)
        return -1;
    bool started = strstr(cur, "rndis") != NULL;
    if (enable == started)
        return 0;
    if (enable) {
        strlcpy(sSavedFunctions, cur, sizeof(sSavedFunctions));
        strlcpy(next, strstr(cur, "adb") ? "rndis,adb" : "rndis", sizeof(next));
    } else if (sSavedFunctions[0] != '\0') {
        strlcpy(next, sSavedFunctions, sizeof(next));
    } else {
        strlcpy(next, strstr(cur, "adb") ? "mass_storage,adb" : "mass_storage",
                sizeof(next));
    }
    LOGD("android_usb functions %s -> %s", cur, next);
    if (writeSys(ANDROID_USB "enable", "0") < 0 ||
            writeSys(ANDROID_USB "functions", next) < 0)
        return -1;
    return writeSys(ANDROID_USB "enable", "1");
}


UsbController::UsbController() {
}

UsbController::~UsbController() {
}

int UsbController::startRNDIS() {
    LOGD("Usb RNDIS start");
    return enableRNDIS(true);
}

int UsbController::stopRNDIS() {
    LOGD("Usb RNDIS stop");
    return enableRNDIS(false);
}

int UsbController::enableRNDIS(bool enable) {
    char value[20];
    if (hasAndroidUsb())
        return enableAndroidUsbRNDIS(enable);
#ifdef USE_HTC_USB_FUNCTION_SWITCH
    int fd = open("/sys/devices/platform/msm_hsusb/usb_function_switch", O_RDWR);
    int count = snprintf(value, sizeof(value), "%d\n", (enable ? 4 : 3));
#else
    int fd = open("/sys/class/usb_composite/rndis/enable", O_RDWR);
    int count = snprintf(value, sizeof(value), "%d\n", (enable ? 1 : 0));
#endif
    write(fd, value, count);
    close(fd);
    return 0;
}

bool UsbController::isRNDISStarted() {
    char value=0;
    if (hasAndroidUsb()) {
        char cur[256];
        return readSys(ANDROID_USB "functions", cur, sizeof(cur)) == 0 &&
                strstr(cur, "rndis") != NULL;
    }
#ifdef USE_HTC_USB_FUNCTION_SWITCH
    int fd = open("/sys/devices/platform/msm_hsusb/usb_function_switch", O_RDWR);
#else
    int fd = open("/sys/class/usb_composite/rndis/enable", O_RDWR);
#endif
    read(fd, &value, 1);
    close(fd);
#ifdef USE_HTC_USB_FUNCTION_SWITCH
    return (value == '4' ? true : false);
#else
    return (value == '1' ? true : false);
#endif
}
