#ifndef ZB_INFO_H_
#define ZB_INFO_H_

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <ArduinoJson.h>

#include "zb_ver.h"

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

// Detection lifecycle for the values above:
//   UNKNOWN   - every attempt so far finished with nothing detected, no cache
//   DETECTING - a live query is in flight or scheduled this boot, nothing known
//   KNOWN     - values from the cache loaded at boot or a successful live read
enum class ZbDetectState : uint8_t
{
    UNKNOWN,
    DETECTING,
    KNOWN
};

// Wire sentinels for the web UI (lowercase English tokens, like "unknown").
constexpr const char *ZB_INFO_CHECKING = "checking";
constexpr const char *ZB_INFO_UNKNOWN = "unknown";

// Sentinel to emit instead of a value when there is nothing to show yet:
// "checking" while detection is pending, otherwise "unknown".
inline const char *zbDetectSentinel(ZbDetectState state) noexcept
{
    return state == ZbDetectState::DETECTING ? ZB_INFO_CHECKING : ZB_INFO_UNKNOWN;
}

// Wire text for the coordinator revision: "checking" while detection is
// pending, "unknown" when nothing is known, else the decimal revision.
// `buf` backs the formatted number and must outlive the returned pointer.
inline const char *zbRevDisplay(ZbDetectState state, uint32_t rev, char *buf, size_t bufLen) noexcept
{
    if (state == ZbDetectState::KNOWN && rev > 0 && bufLen > 0)
    {
        snprintf(buf, bufLen, "%lu", static_cast<unsigned long>(rev));
        return buf;
    }
    return zbDetectSentinel(state);
}

// Wire text for the coordinator role: "checking" while detection is pending,
// the role name once known, "unknown" when no role was detected.
inline const char *zbRoleDisplay(ZbDetectState state, uint8_t deviceState) noexcept
{
    if (state != ZbDetectState::KNOWN)
    {
        return zbDetectSentinel(state);
    }
    return zbRoleName(zbRoleFromDeviceState(deviceState));
}

// State after an attempt finishes: success -> KNOWN; failure keeps KNOWN when
// values were already known (cache/earlier read), otherwise falls to UNKNOWN.
inline ZbDetectState zbDetectAfterAttempt(ZbDetectState previous, bool success) noexcept
{
    if (success)
        return ZbDetectState::KNOWN;
    return previous == ZbDetectState::KNOWN ? ZbDetectState::KNOWN : ZbDetectState::UNKNOWN;
}

// Detection state for the global zbVer values (defined in zb_info.cpp).
extern ZbDetectState zbInfoDetectState;

// LittleFS persistence (implemented in zb_info.cpp):
// loadZbInfo() at boot, saveZbInfo() after a live detection. saveZbInfo()
// skips the write when the values are unchanged (LittleFS wear).
bool loadZbInfo();
void saveZbInfo();
bool zbInfoKnown();

#endif
