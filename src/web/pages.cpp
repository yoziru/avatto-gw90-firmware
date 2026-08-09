#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <ETH.h>
#include <WiFi.h>
#include <HTTPClient.h>

#include "config.h"
#include "web.h"
#include "etc.h"
#include "zb.h"
#include "zones.h"
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

extern struct ConfigSettingsStruct ConfigSettings;
extern struct zbVerStruct zbVer;
extern struct MqttSettingsStruct MqttSettings;
extern struct WgSettingsStruct WgSettings;

DynamicJsonDocument getRootData()
{
    DynamicJsonDocument doc(1024);

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
            doc[connectedSocketStatus] = "Yes, " + String(ConfigSettings.connectedClients) + "connection";
            doc[connectedSocket] = readableTime;
        }
        else
        {
            doc[connectedSocketStatus] = "Yes, " + String(ConfigSettings.connectedClients) + " connections";
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
    case COORDINATOR_MODE_USB:
        doc[operationalMode] = "Zigbee-to-USB";
        break;
    case COORDINATOR_MODE_WIFI:
        doc[operationalMode] = "Zigbee-to-WiFi";
        break;
    case COORDINATOR_MODE_LAN:
        doc[operationalMode] = "Zigbee-to-Ethernet";
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
    if (zbVer.zbRev > 0)
    {
        doc["zigbeeFwRev"] = String(zbVer.zbRev);
    }
    else
    {
        doc["zigbeeFwRev"] = "unknown";
    }
    if (zbVer.chipID)
    {
        doc["zigbeeHwRev"] = String(zbVer.chipID);
    }
    else
    {
        doc["zigbeeHwRev"] = "unknown";
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
    doc["wifiMac"] = String(WiFi.macAddress().c_str());
    if (ConfigSettings.coordinator_mode == COORDINATOR_MODE_WIFI)
    {
        doc[wifiEnabled] = yes;
        doc[wifiMode] = "Client";
        if (WiFi.status() == WL_CONNECTED)
        {
            String rssiWifi = String(WiFi.RSSI()) + String(" dBm");
            doc[wifiSsid] = WiFi.SSID();
            doc[wifiRssi] = rssiWifi;
            doc[wifiConnected] = "Connected to " + WiFi.SSID();
            doc[wifiIp] = WiFi.localIP().toString();
            doc[wifiMask] = WiFi.subnetMask().toString();
            doc[wifiGate] = WiFi.gatewayIP().toString();
        }
        else
        {
            const char *connecting = "Connecting...";
            doc["wifiSsid"] = ConfigSettings.ssid;
            doc[wifiRssi] = connecting;
            doc[wifiConnected] = connecting;
            doc[wifiIp] = ConfigSettings.dhcpWiFi ? WiFi.localIP().toString() : connecting;
            doc[wifiMask] = ConfigSettings.dhcpWiFi ? WiFi.subnetMask().toString() : connecting;
            doc[wifiGate] = ConfigSettings.dhcpWiFi ? WiFi.gatewayIP().toString() : connecting;
        }
        if (ConfigSettings.dhcpWiFi)
        {
            doc[wifiDhcp] = on;
        }
        else
        {
            doc[wifiDhcp] = off;
        }
    }
    else
    {
        doc[wifiEnabled] = no;
        doc[wifiConnected] = notConnected;
        doc[wifiMode] = off;
        doc[wifiSsid] = off;
        doc[wifiIp] = off;
        doc[wifiMask] = off;
        doc[wifiGate] = off;
        doc[wifiRssi] = off;
        doc[wifiDhcp] = off;
    }
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
    DynamicJsonDocument doc(1024);
    doc = getRootData();
    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);
}

void handleStatus()
{
    String result;
    DynamicJsonDocument doc(1024);

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

    doc = getRootData();
    serializeJsonPretty(doc, result);
    serverWeb.send(HTTP_CODE_OK, contTypeJson, result);
}

void handleGeneral()
{
    DynamicJsonDocument doc(1024);
    String result;

    switch (ConfigSettings.coordinator_mode)
    {
    case COORDINATOR_MODE_USB:
        doc["checkedUsbMode"] = checked;
        break;
    case COORDINATOR_MODE_WIFI:
        doc["checkedWifiMode"] = checked;
        break;
    case COORDINATOR_MODE_LAN:
        doc["checkedLanMode"] = checked;
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
    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);
}

void handleSecurity()
{
    String result;
    DynamicJsonDocument doc(1024);

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
    DynamicJsonDocument doc(1024);

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
    DynamicJsonDocument doc(1024);

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
    DynamicJsonDocument doc(1024);

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
    DynamicJsonDocument doc(1024);

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

    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);
}

void handleWg()
{
    String result;
    DynamicJsonDocument doc(1024);

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
    String result;
    DynamicJsonDocument doc(512);

    doc["hostname"] = ConfigSettings.hostname;
    doc["refreshLogs"] = ConfigSettings.refreshLogs;
    if (ConfigSettings.timeZone)
    {
        doc["timeZoneName"] = ConfigSettings.timeZone;
    }
    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);

    DynamicJsonDocument zones(10240);
    String results;

    JsonArray zonesArray = zones.to<JsonArray>();
    for (int i = 0; i < timeZoneCount; i++)
    {
        zonesArray.add(timeZones[i].zone);
    }

    serializeJson(zones, results);
    serverWeb.sendHeader(respTimeZonesName, results);
}

void handleAbout()
{
    String result;
    DynamicJsonDocument doc(512);

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
    doc["espModel"] = String(ESP.getChipModel());

    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);
}
