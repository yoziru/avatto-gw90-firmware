#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>

#include "config.h"
#include "log.h"
#include "zb_info.h"

extern zbVerStruct zbVer;

namespace
{
const char *const zbInfoFile = "/config/zbInfo.json";

// What is already persisted. Used to skip unchanged writes (LittleFS wear).
ZbInfo zbInfoSaved;
bool zbInfoSavedValid = false;

ZbInfo zbInfoFromVer()
{
    ZbInfo info;
    info.zbRev = zbVer.zbRev;
    info.deviceState = zbVer.deviceState;
    info.maintrel = zbVer.maintrel;
    info.minorrel = zbVer.minorrel;
    info.majorrel = zbVer.majorrel;
    info.product = zbVer.product;
    info.transportrev = zbVer.transportrev;
    snprintf(info.chipID, sizeof(info.chipID), "%s", zbVer.chipID.c_str());
    return info;
}

void zbInfoApply(const ZbInfo &info)
{
    zbVer.zbRev = info.zbRev;
    zbVer.deviceState = info.deviceState;
    zbVer.maintrel = info.maintrel;
    zbVer.minorrel = info.minorrel;
    zbVer.majorrel = info.majorrel;
    zbVer.product = info.product;
    zbVer.transportrev = info.transportrev;
    zbVer.chipID = info.chipID;
}
} // namespace

bool zbInfoKnown()
{
    return zbInfoHasData(zbInfoFromVer());
}

bool loadZbInfo()
{
    File file = LittleFS.open(zbInfoFile, FILE_READ);
    if (!file)
    {
        return false;
    }
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error)
    {
        DEBUG_PRINTLN(F("zbInfo.json corrupt, removing"));
        LittleFS.remove(zbInfoFile);
        return false;
    }

    const ZbInfo info = zbInfoFromJson(doc);
    if (!zbInfoHasData(info))
    {
        return false;
    }
    zbInfoApply(info);
    zbInfoSaved = info;
    zbInfoSavedValid = true;
    return true;
}

void saveZbInfo()
{
    const ZbInfo info = zbInfoFromVer();
    if (!zbInfoHasData(info))
    {
        return; // never persist "unknown"
    }
    if (zbInfoSavedValid && zbInfoEquals(info, zbInfoSaved))
    {
        return; // unchanged - spare the flash
    }

    JsonDocument doc;
    zbInfoToJson(info, doc);
    File file = LittleFS.open(zbInfoFile, FILE_WRITE);
    if (!file)
    {
        DEBUG_PRINTLN(F("failed to write zbInfo.json"));
        return;
    }
    serializeJson(doc, file);
    file.close();
    zbInfoSaved = info;
    zbInfoSavedValid = true;
}
