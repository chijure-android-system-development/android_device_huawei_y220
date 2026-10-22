#define LOG_TAG "PropertyMap"

#include <stdlib.h>
#include <utils/Log.h>

#include "PropertyMap.h"

namespace android {

PropertyMap::PropertyMap() {}
PropertyMap::~PropertyMap() {}

void PropertyMap::clear() {
    mProperties.clear();
}

void PropertyMap::addProperty(const String8& key, const String8& value) {
    mProperties.add(key, value);
}

bool PropertyMap::hasProperty(const String8& key) const {
    return mProperties.indexOfKey(key) >= 0;
}

bool PropertyMap::tryGetProperty(const String8& key, String8& outValue) const {
    ssize_t index = mProperties.indexOfKey(key);
    if (index < 0)
        return false;
    outValue = mProperties.valueAt(index);
    return true;
}

bool PropertyMap::tryGetProperty(const String8& key, bool& outValue) const {
    int32_t value;
    if (!tryGetProperty(key, value))
        return false;
    outValue = value;
    return true;
}

bool PropertyMap::tryGetProperty(const String8& key, int32_t& outValue) const {
    String8 stringValue;
    if (!tryGetProperty(key, stringValue) || stringValue.length() == 0)
        return false;

    char* end;
    int value = strtol(stringValue.string(), &end, 10);
    if (*end != '\0') {
        LOGW("Property key '%s' has invalid value '%s'. Expected an integer.",
                key.string(), stringValue.string());
        return false;
    }
    outValue = value;
    return true;
}

bool PropertyMap::tryGetProperty(const String8& key, float& outValue) const {
    String8 stringValue;
    if (!tryGetProperty(key, stringValue) || stringValue.length() == 0)
        return false;

    char* end;
    float value = strtof(stringValue.string(), &end);
    if (*end != '\0') {
        LOGW("Property key '%s' has invalid value '%s'. Expected a float.",
                key.string(), stringValue.string());
        return false;
    }
    outValue = value;
    return true;
}

} // namespace android
