/*
 * The Y220's own libcameracustom (GB stock, the only one that knows its
 * gc0329 sensor) misses two calls that the Y330 camera blobs make. Both
 * are flash/AF lamp queries; in the Y330 library they return 0, and the
 * gc0329 has no AF lamp.
 */

namespace NSCamCustom {

unsigned int custom_GetYuvPreflashAF()
{
    return 0;
}

unsigned int custom_GetYuvAfLampSupport()
{
    return 0;
}

}; // namespace NSCamCustom
