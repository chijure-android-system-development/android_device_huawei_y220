#ifndef Y220_UTILS_PROPERTY_MAP_H
#define Y220_UTILS_PROPERTY_MAP_H

#include <utils/KeyedVector.h>
#include <utils/String8.h>

namespace android {

/* ICS-compatible layout required by the MediaTek camera blobs. */
class PropertyMap {
public:
    PropertyMap();
    ~PropertyMap();

    void clear();
    void addProperty(const String8& key, const String8& value);
    bool hasProperty(const String8& key) const;
    bool tryGetProperty(const String8& key, String8& outValue) const;
    bool tryGetProperty(const String8& key, bool& outValue) const;
    bool tryGetProperty(const String8& key, int32_t& outValue) const;
    bool tryGetProperty(const String8& key, float& outValue) const;

    inline const KeyedVector<String8, String8>& getProperties() const {
        return mProperties;
    }

private:
    KeyedVector<String8, String8> mProperties;
};

} // namespace android

#endif
