#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

struct Y220Yv12Converter {
    uint16_t* buffer;
    size_t pixels;
};

extern "C" void y220_yv12_converter_destroy(void* converter)
{
    Y220Yv12Converter* state = static_cast<Y220Yv12Converter*>(converter);
    if (state) {
        free(state->buffer);
        delete state;
    }
}

extern "C" bool y220_yv12_to_rgb565(void** converter, const uint8_t* src,
        int stride, int vstride, int width, int height,
        const uint16_t** output)
{
    if (!converter || !src || !output || stride <= 0 || vstride <= 0 ||
            width <= 0 || height <= 0 || width > stride || height > vstride) {
        return false;
    }

    Y220Yv12Converter* state = static_cast<Y220Yv12Converter*>(*converter);
    if (!state) {
        state = new Y220Yv12Converter;
        state->buffer = 0;
        state->pixels = 0;
        *converter = state;
    }

    const size_t pixels = static_cast<size_t>(width) * height;
    if (state->pixels < pixels) {
        uint16_t* buffer = static_cast<uint16_t*>(
                realloc(state->buffer, pixels * sizeof(uint16_t)));
        if (!buffer)
            return false;
        state->buffer = buffer;
        state->pixels = pixels;
    }

    const int chromaStride = ((stride / 2) + 15) & ~15;
    const uint8_t* yPlane = src;
    const uint8_t* vPlane = src + stride * vstride;
    const uint8_t* uPlane = vPlane + chromaStride * (vstride / 2);
    uint16_t* dst = state->buffer;

    for (int y = 0; y < height; y++) {
        const uint8_t* yRow = yPlane + y * stride;
        const uint8_t* vRow = vPlane + (y / 2) * chromaStride;
        const uint8_t* uRow = uPlane + (y / 2) * chromaStride;
        for (int x = 0; x < width; x++) {
            int c = 298 * (yRow[x] - 16);
            int d = uRow[x / 2] - 128;
            int e = vRow[x / 2] - 128;
            int r = (c + 409 * e + 128) >> 8;
            int g = (c - 100 * d - 208 * e + 128) >> 8;
            int b = (c + 516 * d + 128) >> 8;
            r = r < 0 ? 0 : (r > 255 ? 255 : r);
            g = g < 0 ? 0 : (g > 255 ? 255 : g);
            b = b < 0 ? 0 : (b > 255 ? 255 : b);
            *dst++ = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        }
    }

    *output = state->buffer;
    return true;
}
