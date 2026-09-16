#ifndef ZB_INFO_H_
#define ZB_INFO_H_

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <ArduinoJson.h>

// Last-known CC2652P hardware/firmware info. Persisted in /config/zbInfo.json
// (see zb_info.cpp) so the web UI can show it immediately after boot instead
// of "unknown" until the coordinator answers. Cached values are display-only:
// Zigbee flashing requires a live role detection in the current boot.
struct ZbInfo
{
    uint32_t zbRev = 0;
    uint8_t deviceState = 0xFF;
    uint8_t maintrel = 0;
    uint8_t minorrel = 0;
    uint8_t majorrel = 0;
    uint8_t product = 0;
    uint8_t transportrev = 0;
    char chipID[16] = {0};
};

// True when anything about the coordinator was ever detected (cache or live).
// A refresh keeps showing these values instead of blanking to "unknown".
inline bool zbInfoHasData(const ZbInfo &info) noexcept
{
    return info.zbRev > 0 || info.deviceState != 0xFF || info.chipID[0] != '\0';
}

inline bool zbInfoEquals(const ZbInfo &a, const ZbInfo &b) noexcept
{
    return a.zbRev == b.zbRev &&
           a.deviceState == b.deviceState &&
           a.maintrel == b.maintrel &&
           a.minorrel == b.minorrel &&
           a.majorrel == b.majorrel &&
           a.product == b.product &&
           a.transportrev == b.transportrev &&
           strcmp(a.chipID, b.chipID) == 0;
}

inline void zbInfoToJson(const ZbInfo &info, JsonDocument &doc) noexcept
{
    doc["zbRev"] = info.zbRev;
    doc["deviceState"] = info.deviceState;
    doc["maintrel"] = info.maintrel;
    doc["minorrel"] = info.minorrel;
    doc["majorrel"] = info.majorrel;
    doc["product"] = info.product;
    doc["transportrev"] = info.transportrev;
    doc["chipID"] = info.chipID;
}

inline ZbInfo zbInfoFromJson(const JsonVariantConst &doc) noexcept
{
    ZbInfo info;
    info.zbRev = doc["zbRev"] | 0;
    info.deviceState = doc["deviceState"] | 0xFF;
    info.maintrel = doc["maintrel"] | 0;
    info.minorrel = doc["minorrel"] | 0;
    info.majorrel = doc["majorrel"] | 0;
    info.product = doc["product"] | 0;
    info.transportrev = doc["transportrev"] | 0;
    const char *chipID = doc["chipID"] | "";
    snprintf(info.chipID, sizeof(info.chipID), "%s", chipID);
    return info;
}

// LittleFS persistence (implemented in zb_info.cpp):
// loadZbInfo() at boot, saveZbInfo() after a live detection. saveZbInfo()
// skips the write when the values are unchanged (LittleFS wear).
bool loadZbInfo();
void saveZbInfo();
bool zbInfoKnown();

#endif
