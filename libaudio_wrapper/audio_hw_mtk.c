/*
 * audio.primary.mt6572: hands the audio HAL module over to the stock MTK
 * HAL (Y330-U01 4.2.2, /system/lib/libaudio.primary.default.so).
 *
 * MTK's libhardware loads that library itself, and its module id is
 * "libaudio". CM's hw_get_module_by_class() wants "audio" and refused it
 * (-EINVAL), so AudioFlinger had no primary output. This module carries
 * the right id and opens the device through the MTK module's methods.
 * Same approach as the audio wrapper of android_device_alps_m706 (CM11).
 *
 * Capture: the MTK input stream only opens the microphone as stereo at
 * 48 kHz (AudioMTKStreamIn::Set/CheckSampleRate) and leaves conversion to
 * MTK's own AudioFlinger. CM's AudioFlinger retries a refused input only
 * when the suggested rate is at most twice the requested one, so 8 and
 * 16 kHz recording failed. Refused inputs are opened here at the HAL's
 * suggested config and converted on read to what AudioFlinger asked for:
 * channels first, then the rate (speex resampler from libaudioutils).
 */

#define LOG_TAG "audio_hw_mtk"

#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <cutils/log.h>
#include <hardware/audio.h>
#include <hardware/hardware.h>
#include <audio_utils/resampler.h>

#define MTK_AUDIO_HAL "libaudio.primary.default.so"

typedef int (*open_input_t)(struct audio_hw_device *dev,
                            audio_io_handle_t handle,
                            audio_devices_t devices,
                            struct audio_config *config,
                            struct audio_stream_in **stream_in);
typedef void (*close_input_t)(struct audio_hw_device *dev,
                              struct audio_stream_in *stream_in);

static open_input_t mtk_open_input;
static close_input_t mtk_close_input;

struct conv_in {
    struct audio_stream_in stream;      /* what AudioFlinger sees */
    struct audio_stream_in *mtk;        /* the HAL's stream */
    struct resampler_buffer_provider provider;
    struct resampler_itfe *resampler;   /* NULL when the rates match */

    uint32_t hw_rate;
    int hw_channels;
    uint32_t rate;                      /* requested */
    audio_channel_mask_t mask;
    int channels;

    int16_t *raw;                       /* one HAL read, hw_channels */
    int16_t *conv;                      /* the same frames, channels */
    size_t chunk_frames;                /* frames per HAL read */
    size_t avail;                       /* converted frames not consumed */
    size_t pos;
    int error;
};

#define TO_CONV(s) ((struct conv_in *) (s))
#define MTK_COMMON(s) (&TO_CONV(s)->mtk->common)

/* Reads one HAL buffer and converts its channels into conv. */
static int fill(struct conv_in *in)
{
    ssize_t bytes = in->mtk->read(in->mtk, in->raw,
                                  in->chunk_frames * in->hw_channels * sizeof(int16_t));
    size_t frames, i;

    if (bytes <= 0)
        return bytes < 0 ? (int) bytes : -EIO;
    frames = bytes / (in->hw_channels * sizeof(int16_t));

    if (in->hw_channels == in->channels) {
        memcpy(in->conv, in->raw, frames * in->channels * sizeof(int16_t));
    } else if (in->hw_channels == 2 && in->channels == 1) {
        for (i = 0; i < frames; i++)
            in->conv[i] = ((int) in->raw[2 * i] + in->raw[2 * i + 1]) / 2;
    } else {
        for (i = 0; i < frames; i++)
            in->conv[2 * i] = in->conv[2 * i + 1] = in->raw[i];
    }
    in->avail = frames;
    in->pos = 0;
    return 0;
}

static int get_next_buffer(struct resampler_buffer_provider *provider,
                           struct resampler_buffer *buffer)
{
    struct conv_in *in = (struct conv_in *)
        ((char *) provider - offsetof(struct conv_in, provider));

    if (in->avail == 0) {
        in->error = fill(in);
        if (in->error) {
            buffer->raw = NULL;
            buffer->frame_count = 0;
            return in->error;
        }
    }
    if (buffer->frame_count > in->avail)
        buffer->frame_count = in->avail;
    buffer->i16 = in->conv + in->pos * in->channels;
    return 0;
}

static void release_buffer(struct resampler_buffer_provider *provider,
                           struct resampler_buffer *buffer)
{
    struct conv_in *in = (struct conv_in *)
        ((char *) provider - offsetof(struct conv_in, provider));

    in->pos += buffer->frame_count;
    in->avail -= buffer->frame_count;
}

static ssize_t conv_read(struct audio_stream_in *stream, void *buffer, size_t bytes)
{
    struct conv_in *in = TO_CONV(stream);
    size_t want = bytes / (in->channels * sizeof(int16_t));
    int16_t *out = buffer;
    size_t done = 0;

    in->error = 0;
    while (done < want) {
        size_t frames = want - done;

        if (in->resampler != NULL) {
            in->resampler->resample_from_provider(in->resampler,
                                                  out + done * in->channels, &frames);
        } else {
            struct resampler_buffer buf = { { NULL }, frames };

            if (get_next_buffer(&in->provider, &buf) == 0 && buf.frame_count > 0) {
                memcpy(out + done * in->channels, buf.i16,
                       buf.frame_count * in->channels * sizeof(int16_t));
                frames = buf.frame_count;
                release_buffer(&in->provider, &buf);
            } else {
                frames = 0;
            }
        }
        if (in->error)
            return in->error;
        if (frames == 0)
            break;
        done += frames;
    }
    return done * in->channels * sizeof(int16_t);
}

static uint32_t conv_get_sample_rate(const struct audio_stream *stream)
{
    return TO_CONV(stream)->rate;
}

static int conv_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    return -ENOSYS;
}

/* The HAL buffer scaled to the requested rate and channel count. */
static size_t conv_get_buffer_size(const struct audio_stream *stream)
{
    const struct conv_in *in = TO_CONV(stream);
    size_t frames = (size_t) ((uint64_t) in->chunk_frames * in->rate / in->hw_rate);

    frames &= ~(size_t) 15;
    if (frames == 0)
        frames = 16;
    return frames * in->channels * sizeof(int16_t);
}

static audio_channel_mask_t conv_get_channels(const struct audio_stream *stream)
{
    return TO_CONV(stream)->mask;
}

static audio_format_t conv_get_format(const struct audio_stream *stream)
{
    return AUDIO_FORMAT_PCM_16_BIT;
}

static int conv_set_format(struct audio_stream *stream, audio_format_t format)
{
    return -ENOSYS;
}

static int conv_standby(struct audio_stream *stream)
{
    struct conv_in *in = TO_CONV(stream);

    in->avail = 0;
    if (in->resampler != NULL)
        in->resampler->reset(in->resampler);
    return in->mtk->common.standby(&in->mtk->common);
}

static int conv_dump(const struct audio_stream *stream, int fd)
{
    return MTK_COMMON(stream)->dump(MTK_COMMON(stream), fd);
}

static audio_devices_t conv_get_device(const struct audio_stream *stream)
{
    return MTK_COMMON(stream)->get_device(MTK_COMMON(stream));
}

static int conv_set_device(struct audio_stream *stream, audio_devices_t device)
{
    return MTK_COMMON(stream)->set_device(MTK_COMMON(stream), device);
}

static int conv_set_parameters(struct audio_stream *stream, const char *kv_pairs)
{
    return MTK_COMMON(stream)->set_parameters(MTK_COMMON(stream), kv_pairs);
}

static char *conv_get_parameters(const struct audio_stream *stream, const char *keys)
{
    return MTK_COMMON(stream)->get_parameters(MTK_COMMON(stream), keys);
}

static int conv_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return MTK_COMMON(stream)->add_audio_effect(MTK_COMMON(stream), effect);
}

static int conv_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return MTK_COMMON(stream)->remove_audio_effect(MTK_COMMON(stream), effect);
}

static int conv_set_gain(struct audio_stream_in *stream, float gain)
{
    struct conv_in *in = TO_CONV(stream);

    return in->mtk->set_gain(in->mtk, gain);
}

static uint32_t conv_get_input_frames_lost(struct audio_stream_in *stream)
{
    struct conv_in *in = TO_CONV(stream);

    return in->mtk->get_input_frames_lost(in->mtk);
}

static void free_conv(struct conv_in *in)
{
    if (in->resampler != NULL)
        release_resampler(in->resampler);
    free(in->raw);
    free(in->conv);
    free(in);
}

static int adev_open_input_stream(struct audio_hw_device *dev,
                                  audio_io_handle_t handle,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in)
{
    struct audio_config req = *config;
    struct audio_config hw;
    struct conv_in *in;
    int ret, channels;

    ret = mtk_open_input(dev, handle, devices, config, stream_in);
    if (ret == 0)
        return 0;

    /* The HAL wrote what it would take into config. */
    hw = *config;
    *config = req;
    channels = popcount(req.channel_mask);
    if (req.format != AUDIO_FORMAT_PCM_16_BIT || hw.format != AUDIO_FORMAT_PCM_16_BIT ||
        channels < 1 || channels > 2 || popcount(hw.channel_mask) > 2 ||
        hw.sample_rate == 0 || req.sample_rate == 0)
        return ret;

    in = calloc(1, sizeof(*in));
    if (in == NULL)
        return -ENOMEM;

    ret = mtk_open_input(dev, handle, devices, &hw, &in->mtk);
    if (ret != 0) {
        ALOGE("HAL refused its own config: %u Hz mask %#x (%d)",
              hw.sample_rate, hw.channel_mask, ret);
        free(in);
        return ret;
    }

    in->hw_rate = hw.sample_rate;
    in->hw_channels = popcount(hw.channel_mask);
    in->rate = req.sample_rate;
    in->mask = req.channel_mask;
    in->channels = channels;
    in->chunk_frames = in->mtk->common.get_buffer_size(&in->mtk->common) /
                       (in->hw_channels * sizeof(int16_t));
    in->raw = malloc(in->chunk_frames * in->hw_channels * sizeof(int16_t));
    in->conv = malloc(in->chunk_frames * 2 * sizeof(int16_t));
    in->provider.get_next_buffer = get_next_buffer;
    in->provider.release_buffer = release_buffer;

    if (in->raw == NULL || in->conv == NULL ||
        (in->rate != in->hw_rate &&
         create_resampler(in->hw_rate, in->rate, in->channels,
                          RESAMPLER_QUALITY_DEFAULT, &in->provider,
                          &in->resampler) != 0)) {
        mtk_close_input(dev, in->mtk);
        free_conv(in);
        return -ENOMEM;
    }

    in->stream.common.get_sample_rate = conv_get_sample_rate;
    in->stream.common.set_sample_rate = conv_set_sample_rate;
    in->stream.common.get_buffer_size = conv_get_buffer_size;
    in->stream.common.get_channels = conv_get_channels;
    in->stream.common.get_format = conv_get_format;
    in->stream.common.set_format = conv_set_format;
    in->stream.common.standby = conv_standby;
    in->stream.common.dump = conv_dump;
    in->stream.common.get_device = conv_get_device;
    in->stream.common.set_device = conv_set_device;
    in->stream.common.set_parameters = conv_set_parameters;
    in->stream.common.get_parameters = conv_get_parameters;
    in->stream.common.add_audio_effect = conv_add_audio_effect;
    in->stream.common.remove_audio_effect = conv_remove_audio_effect;
    in->stream.set_gain = conv_set_gain;
    in->stream.read = conv_read;
    in->stream.get_input_frames_lost = conv_get_input_frames_lost;

    ALOGI("input %u Hz %d ch from the HAL's %u Hz %d ch",
          in->rate, in->channels, in->hw_rate, in->hw_channels);
    *stream_in = &in->stream;
    return 0;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                    struct audio_stream_in *stream)
{
    struct conv_in *in;

    if (stream->read != conv_read) {
        mtk_close_input(dev, stream);
        return;
    }
    in = TO_CONV(stream);
    mtk_close_input(dev, in->mtk);
    free_conv(in);
}

static int mtk_adev_open(const hw_module_t *module, const char *name,
                         hw_device_t **device)
{
    const struct hw_module_t *mtk;
    struct audio_hw_device *adev;
    void *handle;
    int ret;

    handle = dlopen(MTK_AUDIO_HAL, RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL) {
        ALOGE("cannot load %s: %s", MTK_AUDIO_HAL, dlerror());
        return -EINVAL;
    }

    mtk = (const struct hw_module_t *) dlsym(handle, HAL_MODULE_INFO_SYM_AS_STR);
    if (mtk == NULL || mtk->methods == NULL || mtk->methods->open == NULL) {
        ALOGE("%s has no usable %s", MTK_AUDIO_HAL, HAL_MODULE_INFO_SYM_AS_STR);
        dlclose(handle);
        return -EINVAL;
    }

    /* The MTK device keeps this module as its hw_device_t.module. */
    ret = mtk->methods->open(module, name, device);
    if (ret != 0)
        return ret;

    adev = (struct audio_hw_device *) *device;
    mtk_open_input = adev->open_input_stream;
    mtk_close_input = adev->close_input_stream;
    adev->open_input_stream = adev_open_input_stream;
    adev->close_input_stream = adev_close_input_stream;
    return 0;
}

static struct hw_module_methods_t mtk_audio_module_methods = {
    .open = mtk_adev_open,
};

/* Not const: libhardware writes dso into it. */
struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "MTK audio HAL (Y330 stock)",
        .author = "MediaTek",
        .methods = &mtk_audio_module_methods,
    },
};
