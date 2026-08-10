#ifndef CONFIG_UTILS_H_
#define CONFIG_UTILS_H_

#include <ArduinoJson.h>

// Clamp a persisted temperature offset to a sane range. A stale/bogus value
// (e.g. written by firmware that read the frozen 0x80 TSENS sentinel) would
// otherwise skew every temperature report; returning 0 means "not calibrated"
// so the caller re-calibrates against the ~30 °C reference.
inline int clampTempOffset(int offset) noexcept
{
    if (offset < -10 || offset > 10)
    {
        return 0;
    }
    return offset;
}

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
