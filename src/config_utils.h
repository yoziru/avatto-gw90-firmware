#ifndef CONFIG_UTILS_H_
#define CONFIG_UTILS_H_

#include <ArduinoJson.h>

// The WireGuard private key was historically persisted under the "localIP" key
// (a leftover naming mistake). Read the new "localPrivKey" first and fall back
// to the legacy key so existing configs migrate silently. Returns nullptr when
// neither key holds a string.
inline const char *wgPrivateKey(const JsonVariantConst &doc) noexcept
{
    if (doc["localPrivKey"].is<const char *>())
    {
        return doc["localPrivKey"].as<const char *>();
    }
    if (doc["localIP"].is<const char *>())
    {
        return doc["localIP"].as<const char *>();
    }
    return nullptr;
}

#endif
