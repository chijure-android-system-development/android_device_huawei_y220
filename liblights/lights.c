/*
 * Y220-U05. leds-mt65xx exposes the PMIC ISINK as
 * /sys/class/leds/lcd-backlight/brightness (0-255). The kernel maps that
 * onto the ISINK duty. There is no lights HAL in the stock dump.
 */

#define LOG_TAG "lights"

#include <cutils/log.h>
#include <errno.h>
#include <fcntl.h>
#include <hardware/lights.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static pthread_once_t g_init = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static const char LCD_FILE[] =
        "/sys/class/leds/lcd-backlight/brightness";
static const char BUTTON_FILE[] =
        "/sys/class/leds/button-backlight/brightness";

static void init_globals(void)
{
    pthread_mutex_init(&g_lock, NULL);
}

static int write_int(const char *path, int value)
{
    char buffer[20];
    int fd, len, amt;

    fd = open(path, O_RDWR);
    if (fd < 0) {
        LOGE("open %s failed: %s", path, strerror(errno));
        return -errno;
    }
    len = sprintf(buffer, "%d\n", value);
    amt = write(fd, buffer, len);
    close(fd);
    return amt == len ? 0 : -EIO;
}

static int rgb_to_brightness(const struct light_state_t *state)
{
    int color = state->color & 0x00ffffff;
    return ((77 * ((color >> 16) & 0xff)) +
            (150 * ((color >> 8) & 0xff)) +
            (29 * (color & 0xff))) >> 8;
}

static int set_light_backlight(struct light_device_t *dev,
        const struct light_state_t *state)
{
    int err;
    int brightness = rgb_to_brightness(state);

    pthread_mutex_lock(&g_lock);
    err = write_int(LCD_FILE, brightness);
    pthread_mutex_unlock(&g_lock);
    return err;
}

static int set_light_buttons(struct light_device_t *dev,
        const struct light_state_t *state)
{
    int err;
    int on = (state->color & 0x00ffffff) != 0;

    pthread_mutex_lock(&g_lock);
    err = write_int(BUTTON_FILE, on ? 255 : 0);
    pthread_mutex_unlock(&g_lock);
    return err;
}

static int close_lights(struct light_device_t *dev)
{
    if (dev)
        free(dev);
    return 0;
}

static int open_lights(const struct hw_module_t *module, const char *name,
        struct hw_device_t **device)
{
    int (*set_light)(struct light_device_t *dev,
            const struct light_state_t *state);

    if (strcmp(LIGHT_ID_BACKLIGHT, name) == 0)
        set_light = set_light_backlight;
    else if (strcmp(LIGHT_ID_BUTTONS, name) == 0)
        set_light = set_light_buttons;
    else
        return -EINVAL;

    pthread_once(&g_init, init_globals);

    struct light_device_t *dev = malloc(sizeof(*dev));
    if (dev == NULL)
        return -ENOMEM;
    memset(dev, 0, sizeof(*dev));
    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t *)module;
    dev->common.close = (int (*)(struct hw_device_t *))close_lights;
    dev->set_light = set_light;
    *device = (struct hw_device_t *)dev;
    return 0;
}

static struct hw_module_methods_t lights_module_methods = {
    .open = open_lights,
};

const struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = LIGHTS_HARDWARE_MODULE_ID,
    .name = "Y220 lights",
    .author = "CyanogenMod",
    .methods = &lights_module_methods,
};
