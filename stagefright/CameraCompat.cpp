#include <stdint.h>
#include <string.h>

#include <OMX_IVCommon.h>

extern "C" int32_t board_stagefright_camera_color_format(
        const char* colorFormat)
{
    // MediaTek calls its planar Y, U, V video buffers by this name.
    if (colorFormat && !strcmp(colorFormat, "yuv420i-yyuvyy-3plane"))
        return OMX_COLOR_FormatYUV420Planar;

    return -1;
}
