/*
 * Reset the modem after a modem exception.
 *
 * On MTK the reset after a modem exception is asked for from user space:
 * gsm0710muxd sees the mux channel fail, sets mux.report.case and starts
 * muxreport-daemon, which calls CCCI_IOC_MD_RESET; ccci_mdinit then stops
 * gsm0710muxd and rild, reboots the modem and starts them again. CM's init
 * lets only root set mux.* and start muxreport-daemon, and gsm0710muxd runs
 * as radio, so the modem stayed dead until the phone was rebooted.
 *
 * This waits for /sys/kernel/ccci/boot to show the exception stage (3) and
 * asks for the reset itself. The poll only runs while the AP is awake: a
 * sleeping process does not keep the phone out of suspend.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define LOG_TAG "mdwatch_y220"
#include <cutils/log.h>

#define CCCI_BOOT "/sys/kernel/ccci/boot"
#define CCCI_IOCTL "/dev/ccci_ioctl0"

/* dual_ccci/include/ccci_common.h */
#define CCCI_IOC_MAGIC 'C'
#define CCCI_IOC_MD_RESET _IO(CCCI_IOC_MAGIC, 0)

/* ccci_md.h: MD_BOOT_STAGE_EXCEPTION */
#define MD_STAGE_EXCEPTION 3

#define POLL_S 5
/* Seen twice in a row: the kernel has dumped the exception by then. */
#define EXCEPTION_POLLS 2
/* A reset and the modem's boot take about 30 s. */
#define AFTER_RESET_S 60
/* A modem that keeps crashing is left alone instead of reset in a loop. */
#define MAX_RESETS 3
#define RESET_WINDOW_S (15 * 60)

static int md_stage(void)
{
    char buf[32];
    ssize_t len;
    char *stage;
    int fd = open(CCCI_BOOT, O_RDONLY);

    if (fd < 0)
        return -1;
    len = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (len <= 0)
        return -1;
    buf[len] = '\0';

    /* "md1:<stage>\n" */
    stage = strstr(buf, "md1:");
    if (stage == NULL)
        return -1;
    return stage[4] - '0';
}

static int md_reset(void)
{
    int ret, fd = open(CCCI_IOCTL, O_RDWR);

    if (fd < 0) {
        ALOGE("open %s: %s", CCCI_IOCTL, strerror(errno));
        return -1;
    }
    ret = ioctl(fd, CCCI_IOC_MD_RESET);
    if (ret < 0)
        ALOGE("CCCI_IOC_MD_RESET: %s", strerror(errno));
    close(fd);
    return ret;
}

static time_t now(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec;
}

int main(void)
{
    time_t resets[MAX_RESETS] = { 0 };
    int seen = 0, next = 0;

    for (;;) {
        sleep(POLL_S);

        if (md_stage() != MD_STAGE_EXCEPTION) {
            seen = 0;
            continue;
        }
        if (++seen < EXCEPTION_POLLS)
            continue;
        seen = 0;

        /* resets[next] is the oldest of the last MAX_RESETS. */
        if (resets[next] != 0 && now() - resets[next] < RESET_WINDOW_S) {
            ALOGE("modem exception again, %d resets in %d min; not resetting",
                  MAX_RESETS, RESET_WINDOW_S / 60);
            sleep(RESET_WINDOW_S);
            continue;
        }

        ALOGW("modem exception, resetting the modem");
        if (md_reset() == 0) {
            resets[next] = now();
            next = (next + 1) % MAX_RESETS;
        }
        sleep(AFTER_RESET_S);
    }
    return 0;
}
