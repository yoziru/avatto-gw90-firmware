#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <ETH.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_chip_info.h>
#include <esp_mac.h>
#include <esp_heap_caps.h>

#include "config.h"
#include "web.h"
#include "etc.h"
#include "zb.h"
#include "zb_ver.h"
#include "zones.h"
#include "profiling.h"
#include "version.h"

#include "webh/PAGE_ROOT.html.gz.h"
#include "webh/PAGE_GENERAL.html.gz.h"
#include "webh/PAGE_ETHERNET.html.gz.h"
#include "webh/PAGE_WIFI.html.gz.h"
#include "webh/PAGE_SERIAL.html.gz.h"
#include "webh/PAGE_SECURITY.html.gz.h"
#include "webh/PAGE_SYSTOOLS.html.gz.h"
#include "webh/PAGE_ABOUT.html.gz.h"
#include "webh/PAGE_MQTT.html.gz.h"
#include "webh/PAGE_WG.html.gz.h"

extern zbVerStruct zbVer;

namespace
{
// Reused across page/status requests so the (several-KiB) document pool is
// allocated once and never freed, instead of being churned on every 5 s
// status poll - that alloc/free cycle was the main source of heap
// fragmentation on this gateway. clear() keeps the pool capacity.
JsonDocument s_rootData;
} // namespace

JsonDocument &getRootData()
{
    s_rootData.clear();
    JsonDocument &doc = s_rootData;

    char verArr[48];
    const char *env = STRINGIFY(BUILD_ENV_NAME);

    sprintf(verArr, "%s (%s)", VERSION, env);

    doc["VERSION"] = String(verArr);

    String readableTime;
    getReadableTime(readableTime, ConfigSettings.socketTime);
    const char *connectedSocketStatus = "connectedSocketStatus";
    const char *connectedSocket = "connectedSocket";
    const char *notConnected = "Not connected";
    const char *yes = "Yes";
    const char *no = "No";
    const char *on = "On";
    const char *off = "Off";

    if (ConfigSettings.connectedClients > 0)
    {
        if (ConfigSettings.connectedClients > 1)
        {
            doc[connectedSocketStatus] = "Yes, " + String(ConfigSettings.connectedClients) + " connections";
            doc[connectedSocket] = readableTime;
        }
        else
        {
            doc[connectedSocketStatus] = "Yes, " + String(ConfigSettings.connectedClients) + " connection";
            doc[connectedSocket] = readableTime;
        }
    }
    else
    {
        doc[connectedSocketStatus] = no;
        doc[connectedSocket] = notConnected;
    }
    const char *operationalMode = "operationalMode";
    switch (ConfigSettings.coordinator_mode)
    {
    case CoordinatorMode::PASSTHROUGH:
        doc[operationalMode] = "Zigbee-to-Serial";
        break;
    case CoordinatorMode::NETWORK:
        doc[operationalMode] = "Zigbee-to-Network";
        break;

    default:
        break;
    }

    const char *ethDhcp = "ethDhcp";
    if (ConfigSettings.dhcp)
    {
        doc[ethDhcp] = on;
    }
    else
    {
        doc[ethDhcp] = off;
    }

    const char *connectedEther = "connectedEther";
    const char *ethConnection = "ethConnection";
    const char *ethMac = "ethMac";
    const char *ethSpd = "ethSpd";
    const char *ethIp = "ethIp";
    const char *etchMask = "etchMask";
    const char *ethGate = "ethGate";
    if (ConfigSettings.connectedEther)
    {
        doc[connectedEther] = yes;
        doc[ethConnection] = "Connected";
        doc[ethMac] = ETH.macAddress();
        doc[ethSpd] = String(ETH.linkSpeed()) + String(" Mbps");
        doc[ethIp] = ETH.localIP().toString();
        doc[etchMask] = ETH.subnetMask().toString();
        doc[ethGate] = ETH.gatewayIP().toString();
    }
    else
    {
        doc[connectedEther] = no;
        doc[ethConnection] = notConnected;
        doc[ethMac] = notConnected;
        doc[ethSpd] = notConnected;
        doc[ethIp] = notConnected;
        doc[etchMask] = notConnected;
        doc[ethGate] = notConnected;
    }

    getReadableTime(readableTime, 0);
    doc["uptime"] = readableTime;

    float CPUtemp = getCPUtemp();
    doc["deviceTemp"] = String(CPUtemp, 1);
    doc["hwRev"] = deviceModel;
    doc["espModel"] = String(ESP.getChipModel());
    doc["espCores"] = String(ESP.getChipCores());
    doc["espFreq"] = String(ESP.getCpuFreqMHz());
    doc["espHeapFree"] = String(ESP.getFreeHeap() / 1024);
    doc["espHeapSize"] = String(ESP.getHeapSize() / 1024);
#ifdef BUILD_DEBUG
    // Loop-cycle latency (min / EMA "typical" / max over the current minute)
    // and heap fragmentation. Debug-build only - prod carries no profiler.
    const LoopStats loopStats = profilingGetStats();
    doc["loopMinUs"] = loopStats.cycles ? String(loopStats.minUs) : String(0);
    doc["loopAvgUs"] = loopStats.cycles ? String(loopStats.emaUs) : String(0);
    doc["loopMaxUs"] = loopStats.cycles ? String(loopStats.maxUs) : String(0);
    const size_t heapLargest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    const size_t heapFree = ESP.getFreeHeap();
    const int heapFragPct = (heapFree > 0) ? (int)(100 - (100ULL * heapLargest / heapFree)) : 0;
    doc["espHeapMinFree"] = String(ESP.getMinFreeHeap() / 1024);
    doc["espHeapLargest"] = String(heapLargest / 1024);
    doc["espHeapFrag"] = String(heapFragPct);
#endif
    if (zbVer.zbRev > 0)
    {
        doc["zigbeeFwRev"] = String(zbVer.zbRev);
    }
    else
    {
        doc["zigbeeFwRev"] = "unknown";
    }
    doc["zigbeeFwRole"] = zbRoleName(zbRoleFromDeviceState(zbVer.deviceState));
    if (zbVer.chipID)
    {
        doc["zigbeeHwRev"] = String(zbVer.chipID);
    }
    else
    {
        doc["zigbeeHwRev"] = "unknown";
    }

    const unsigned long nowMs = millis();
    const unsigned long zbSilence = nowMs - zbGetLastRxMs();
    if (ConfigSettings.coordinator_mode == CoordinatorMode::PASSTHROUGH)
    {
        doc["zbStatus"] = "Serial passthrough";
    }
    else if (zbSilence < 15000)
    {
        doc["zbStatus"] = "Active";
    }
    else
    {
        doc["zbStatus"] = "Silent " + String(zbSilence / 1000) + "s";
    }
    doc["zbAutoResets"] = String(zbGetAutoResetCount());

    // Zigbee link between the coordinator and the client (Z2M / ZHA / USB host).
    // The gateway is a transparent bridge: the Zigbee network itself lives in the
    // client app, so we surface the state of the link we actually manage.
    const char *dash = "-"; // shown when a value is not applicable (headers are ASCII-only)
    if (ConfigSettings.coordinator_mode == CoordinatorMode::PASSTHROUGH)
    {
        doc["z2mConnected"] = "USB passthrough";
        doc["z2mClientIp"] = dash;
        doc["z2mClientCount"] = dash;
        doc[connectedSocket] = dash;
    }
    else
    {
        if (ConfigSettings.connectedClients > 0)
        {
            doc["z2mConnected"] = "Connected";
            IPAddress clientIp;
            bool foundIp = false;
            for (int i = 0; i < MAX_SOCKET_CLIENTS; i++)
            {
                if (ConfigSettings.connectedSocket[i] && ConfigSettings.connectedClientIp[i] != IPAddress(0, 0, 0, 0))
                {
                    clientIp = ConfigSettings.connectedClientIp[i];
                    foundIp = true;
                    break;
                }
            }
            doc["z2mClientIp"] = foundIp ? clientIp.toString() : String("Unknown");
            doc["z2mClientCount"] = String(ConfigSettings.connectedClients);
        }
        else
        {
            doc["z2mConnected"] = "Not connected";
            doc["z2mClientIp"] = dash;
            doc["z2mClientCount"] = dash;
            doc[connectedSocket] = dash;
        }
    }

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    const char *espFlashType = "espFlashType";
    if (chip_info.features & CHIP_FEATURE_EMB_FLASH)
    {
        doc[espFlashType] = "embedded";
    }
    else
    {
        doc[espFlashType] = "external";
    }

    doc["espFlashSize"] = String(ESP.getFlashChipSize() / (1024 * 1024));
    const char *wifiSsid = "wifiSsid";
    const char *wifiRssi = "wifiRssi";
    const char *wifiIp = "wifiIp";
    const char *wifiConnected = "wifiConnected";
    const char *wifiMask = "wifiMask";
    const char *wifiGate = "wifiGate";
    const char *wifiEnabled = "wifiEnabled";
    const char *wifiMode = "wifiMode";
    const char *wifiModeAPStatus = "wifiModeAPStatus";
    const char *wifiModeAP = "wifiModeAP";
    const char *wifiDhcp = "wifiDhcp";
    // WiFi.macAddress() returns 00:00:00:00:00:00 when the WiFi interface is off
    // (Ethernet-only mode). Read the burned-in base MAC from eFuse instead.
    uint8_t baseMac[6] = {0};
    esp_base_mac_addr_get(baseMac);
    char wifiMacStr[18] = {0};
    snprintf(wifiMacStr, sizeof(wifiMacStr), "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
    doc["wifiMac"] = String(wifiMacStr);
    if (WiFi.isConnected())
    {
        // WiFi STA is up - either the primary link (SERIAL+keepWeb) or the
        // NETWORK-mode fallback. Report it as an active client.
        doc[wifiEnabled] = yes;
        doc[wifiMode] = "Client";
        String rssiWifi = String(WiFi.RSSI()) + String(" dBm");
        doc[wifiSsid] = WiFi.SSID();
        doc[wifiRssi] = rssiWifi;
        doc[wifiConnected] = "Connected";
        doc[wifiIp] = WiFi.localIP().toString();
        doc[wifiMask] = WiFi.subnetMask().toString();
        doc[wifiGate] = WiFi.gatewayIP().toString();
        if (ConfigSettings.dhcpWiFi)
        {
            doc[wifiDhcp] = on;
        }
        else
        {
            doc[wifiDhcp] = off;
        }
    }
    else if (strlen(ConfigSettings.ssid) >= 2)
    {
        // WiFi credentials are configured but the radio is asleep (Ethernet is
        // the primary uplink; WiFi is only the fallback). Report the radio as
        // off so the user knows it is not actively transmitting.
        doc[wifiEnabled] = no;
        doc[wifiMode] = "Client";
        doc[wifiSsid] = ConfigSettings.ssid;
        doc[wifiRssi] = "N/A";
        doc[wifiConnected] = "Asleep";
        doc[wifiIp] = off;
        doc[wifiMask] = off;
        doc[wifiGate] = off;
        doc[wifiDhcp] = off;
    }
    else
    {
        doc[wifiEnabled] = no;
        doc[wifiConnected] = "Not configured";
        doc[wifiMode] = "Client";
        doc[wifiSsid] = off;
        doc[wifiIp] = off;
        doc[wifiMask] = off;
        doc[wifiGate] = off;
        doc[wifiRssi] = off;
        doc[wifiDhcp] = off;
    }
    // Report the active uplink so the user can see when WiFi fallback is used.
    String netUplink;
    if (ConfigSettings.connectedEther)
    {
        netUplink = "Ethernet";
    }
    else if (WiFi.isConnected())
    {
        netUplink = ConfigSettings.wifiFallbackActive ? "WiFi (fallback)" : "WiFi";
    }
    else if (ConfigSettings.apStarted)
    {
        netUplink = "Access Point";
    }
    else
    {
        netUplink = "None";
    }
    doc["netUplink"] = netUplink;
    if (ConfigSettings.apStarted)
    {
        String AP_NameString;
        char apSsid[18];
        getDeviceID(apSsid);
        char wifiSsidBuf[35];
        sprintf(wifiSsidBuf, "%s (no password)", apSsid);
        doc[wifiSsid] = wifiSsidBuf;
        doc[wifiIp] = "192.168.1.1 (AVATTO-GW90-Ti web interface)";
        doc[wifiMask] = "255.255.255.0 (Access point)";
        doc[wifiGate] = "192.168.1.1 (this device)";
        doc[wifiDhcp] = "On (Access point)";
        doc[wifiModeAPStatus] = "AP started";
        doc[wifiMode] = "AP";
        doc[wifiModeAP] = yes;
        doc[wifiRssi] = "N/A";
    }
    else
    {
        doc[wifiModeAP] = no;
        doc[wifiModeAPStatus] = "Not started";
    }

    const char *wgInit = "wgInit";
    const char *wgDeviceAddr = "wgDeviceAddr";
    const char *wgRemoteAddr = "wgRemoteAddr";

    doc[wgDeviceAddr] = WgSettings.localAddr;
    doc[wgRemoteAddr] = WgSettings.endAddr;

    if (WgSettings.init)
    {
        doc[wgInit] = yes;
    }
    else
    {
        doc[wgInit] = no;
    }
    return doc;
}

void handleRoot()
{
    String result;
    JsonDocument &doc = getRootData();
    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);
}

void handleStatus()
{
    // Reused across polls so the ~1.4 KiB JSON body String isn't allocated and
    // freed every 5 s (this is the /status hot path). Arduino String keeps the
    // reserved capacity across `= ""`.
    static String result;
    if (result.length() == 0)
    {
        result.reserve(2048);
    }

    if (ConfigSettings.webAuth)
    {
        if (!checkAuth())
        {
            serverWeb.sendHeader("Authentication", "fail");
            serverWeb.send(HTTP_CODE_UNAUTHORIZED, contTypeText, F("wrong login or password"));
            return;
        }
        else
        {
            serverWeb.sendHeader("Authentication", "ok");
        }
    }

    JsonDocument &doc = getRootData();
    result = "";
    serializeJsonPretty(doc, result);
    serverWeb.send(HTTP_CODE_OK, contTypeJson, result);
}

void handleGeneral()
{
    JsonDocument doc;
    String result;

    switch (ConfigSettings.coordinator_mode)
    {
    case CoordinatorMode::PASSTHROUGH:
        doc["checkedSerialMode"] = checked;
        break;
    case CoordinatorMode::NETWORK:
        doc["checkedNetworkMode"] = checked;
        break;

    default:
        break;
    }
    if (ConfigSettings.keepWeb)
    {
        doc["keepWeb"] = checked;
    }
    if (ConfigSettings.disableLedPwr)
    {
        doc["checkedDisableLedPwr"] = checked;
    }
    if (ConfigSettings.disableLedUSB)
    {
        doc["checkedDisableLedUSB"] = checked;
    }
    doc["hostname"] = ConfigSettings.hostname;
    doc["refreshLogs"] = ConfigSettings.refreshLogs;
    if (ConfigSettings.timeZone)
    {
        doc["timeZoneName"] = ConfigSettings.timeZone;
    }
    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);

    JsonDocument zones;
    String results;

    JsonArray zonesArray = zones.to<JsonArray>();
    for (int i = 0; i < timeZoneCount; i++)
    {
        zonesArray.add(timeZones[i].zone);
    }

    serializeJson(zones, results);
    serverWeb.sendHeader(respTimeZonesName, results);
}

void handleSecurity()
{
    String result;
    JsonDocument doc;

    if (ConfigSettings.disableWeb)
    {
        doc["disableWeb"] = checked;
    }

    if (ConfigSettings.webAuth)
    {
        doc["webAuth"] = checked;
    }
    doc["webUser"] = (String)ConfigSettings.webUser;
    doc["webPass"] = (String)ConfigSettings.webPass;
    if (ConfigSettings.fwEnabled)
    {
        doc["fwEnabled"] = checked;
    }
    doc["fwIp"] = ConfigSettings.fwIp.toString();

    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);
}

void handleWifi()
{
    String result;
    JsonDocument doc;

    doc["ssid"] = String(ConfigSettings.ssid);
    doc["passWifi"] = String(ConfigSettings.password);
    if (ConfigSettings.dhcpWiFi)
    {
        doc["dchp"] = checked;
    }
    doc["ip"] = ConfigSettings.ipAddressWiFi;
    doc["mask"] = ConfigSettings.ipMaskWiFi;
    doc["gw"] = ConfigSettings.ipGWWiFi;

    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);
}

void handleSerial()
{
    String result;
    JsonDocument doc;

    if (ConfigSettings.serialSpeed == 9600)
    {
        doc["9600"] = checked;
    }
    else if (ConfigSettings.serialSpeed == 19200)
    {
        doc["19200"] = checked;
    }
    else if (ConfigSettings.serialSpeed == 38400)
    {
        doc["8400"] = checked;
    }
    else if (ConfigSettings.serialSpeed == 57600)
    {
        doc["57600"] = checked;
    }
    else if (ConfigSettings.serialSpeed == 115200)
    {
        doc["115200"] = checked;
    }
    else
    {
        doc["115200"] = checked;
    }
    doc["socketPort"] = String(ConfigSettings.socketPort);

    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);
}

void handleEther()
{
    String result;
    JsonDocument doc;

    if (ConfigSettings.dhcp)
    {
        doc["modeEther"] = checked;
    }
    doc["ipEther"] = ConfigSettings.ipAddress;
    doc["maskEther"] = ConfigSettings.ipMask;
    doc["GWEther"] = ConfigSettings.ipGW;

    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);
}

void handleMqtt()
{
    String result;
    JsonDocument doc;

    if (MqttSettings.enable)
    {
        doc["enableMqtt"] = checked;
    }
    doc["serverMqtt"] = MqttSettings.server;
    doc["portMqtt"] = MqttSettings.port;
    doc["userMqtt"] = MqttSettings.user;
    doc["passMqtt"] = MqttSettings.pass;
    doc["topicMqtt"] = MqttSettings.topic;
    doc["intervalMqtt"] = MqttSettings.interval;

    if (MqttSettings.discovery)
    {
        doc["discoveryMqtt"] = checked;
    }

    if (MqttSettings.tls)
    {
        doc["tlsMqtt"] = checked;
    }

    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);
}

void handleWg()
{
    String result;
    JsonDocument doc;

    if (WgSettings.enable)
    {
        doc["enableWg"] = checked;
    }
    doc["localAddrWg"] = WgSettings.localAddr;
    doc["localPrivKeyWg"] = WgSettings.localPrivKey;
    doc["endAddrWg"] = WgSettings.endAddr;
    doc["endPubKeyWg"] = WgSettings.endPubKey;
    doc["endPortWg"] = WgSettings.endPort;

    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);
}

void handleSysTools()
{
    // No per-page values: static page, logs poll via the API.
    serverWeb.sendHeader(respHeaderName, "{}");
}

void handleAbout()
{
    String result;
    JsonDocument &doc = getRootData();

    doc["firmwareVersion"] = String(VERSION);
    String buildTimestamp = String(BUILD_TIMESTAMP);
    String buildDate = "Unknown";

    int spaceIndex = buildTimestamp.indexOf(' ');
    if (spaceIndex != -1) {
        String datePart = buildTimestamp.substring(0, spaceIndex);

        datePart.replace('-', '/');

        buildDate = datePart;
    }

    doc["buildDate"] = buildDate;

    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);
}
