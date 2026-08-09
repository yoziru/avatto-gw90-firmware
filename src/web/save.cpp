#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <HTTPClient.h>

#include "config.h"
#include "web.h"
#include "etc.h"

extern struct ConfigSettingsStruct ConfigSettings;
extern struct MqttSettingsStruct MqttSettings;
extern struct WgSettingsStruct WgSettings;

extern bool wifiWebSetupInProgress;
extern bool loadConfigMqtt();
extern bool loadConfigWg();

void handleSaveParams()
{
    String result;
    DynamicJsonDocument doc(512);
    const char *pageId = "pageId";
    const char *on = "on";
    File configFile;
    const uint8_t one = 1;
    const uint8_t zero = 0;
    if (serverWeb.hasArg(pageId))
    {
        switch (serverWeb.arg(pageId).toInt())
        {
        case API_PAGE_GENERAL:
        {
            configFile = LittleFS.open(configFileGeneral, FILE_READ);
            deserializeJson(doc, configFile);
            configFile.close();

            if (serverWeb.hasArg(coordMode))
            {
                const uint8_t mode = serverWeb.arg(coordMode).toInt();
                DEBUG_PRINTLN(mode);
                if (mode <= 2 && mode >= zero)
                {
                    if (mode == 1)
                    {
                        DEBUG_PRINTLN("mode == 1");
                        wifiWebSetupInProgress = true;
                    }
                    doc[coordMode] = static_cast<uint8_t>(mode);
                    DEBUG_PRINTLN("doc[coordMode]");
                }
            }
            const char *keepWeb = "keepWeb";
            if (serverWeb.arg(keepWeb) == on)
            {
                doc[keepWeb] = one;
            }
            else
            {
                doc[keepWeb] = zero;
            }
            const char *disableLedPwr = "disableLedPwr";
            if (serverWeb.arg(disableLedPwr) == on)
            {
                doc[disableLedPwr] = one;
            }
            else
            {
                doc[disableLedPwr] = zero;
            }
            const char *disableLedUSB = "disableLedUSB";
            if (serverWeb.arg(disableLedUSB) == on)
            {
                doc[disableLedUSB] = one;
            }
            else
            {
                doc[disableLedUSB] = zero;
            }
            configFile = LittleFS.open(configFileGeneral, FILE_WRITE);
            serializeJson(doc, configFile);
            configFile.close();
        }
        break;
        case API_PAGE_ETHERNET:
        {
            configFile = LittleFS.open(configFileEther, FILE_READ);
            deserializeJson(doc, configFile);
            configFile.close();
            doc["ip"] = serverWeb.arg("ipAddress");
            doc["mask"] = serverWeb.arg("ipMask");
            doc["gw"] = serverWeb.arg("ipGW");
            const char *dhcp = "dhcp";
            if (serverWeb.arg(dhcp) == "on")
            {
                doc[dhcp] = one;
            }
            else
            {
                doc[dhcp] = zero;
            }
            configFile = LittleFS.open(configFileEther, FILE_WRITE);
            serializeJson(doc, configFile);
            configFile.close();
        }
        break;
        case API_PAGE_MQTT:
        {
            configFile = LittleFS.open(configFileMqtt, FILE_READ);
            deserializeJson(doc, configFile);
            configFile.close();
            doc["server"] = serverWeb.arg("MqttServer");
            doc["port"] = serverWeb.arg("MqttPort");
            doc["user"] = serverWeb.arg("MqttUser");
            doc["pass"] = serverWeb.arg("MqttPass");
            doc["topic"] = serverWeb.arg("MqttTopic");
            doc["interval"] = serverWeb.arg("MqttInterval");

            const char *enable = "enable";
            if (serverWeb.arg("MqttEnable") == "on")
            {
                doc[enable] = one;
            }
            else
            {
                doc[enable] = zero;
            }

            const char *discovery = "discovery";
            if (serverWeb.arg("MqttDiscovery") == "on")
            {
                doc[discovery] = one;
            }
            else
            {
                doc[discovery] = zero;
            }

            configFile = LittleFS.open(configFileMqtt, FILE_WRITE);
            serializeJson(doc, configFile);
            configFile.close();
            loadConfigMqtt();
        }
        break;
        case API_PAGE_WG:
        {
            configFile = LittleFS.open(configFileWg, FILE_READ);
            deserializeJson(doc, configFile);
            configFile.close();
            doc["localAddr"] = serverWeb.arg("WgLocalAddr");
            doc["localIP"] = serverWeb.arg("WgLocalPrivKey");
            doc["endAddr"] = serverWeb.arg("WgEndAddr");
            doc["endPubKey"] = serverWeb.arg("WgEndPubKey");
            doc["endPort"] = serverWeb.arg("WgEndPort");

            const char *enable = "enable";
            if (serverWeb.arg("WgEnable") == "on")
            {
                doc[enable] = one;
            }
            else
            {
                doc[enable] = zero;
            }

            configFile = LittleFS.open(configFileWg, FILE_WRITE);
            serializeJson(doc, configFile);
            configFile.close();
            loadConfigWg();
        }
        break;
        case API_PAGE_WIFI:
        {
            configFile = LittleFS.open(configFileWifi, FILE_READ);
            deserializeJson(doc, configFile);
            configFile.close();
            doc["ssid"] = serverWeb.arg("WIFISSID");
            doc["pass"] = serverWeb.arg("WIFIpassword");
            const char *dhcpWiFi = "dhcpWiFi";
            if (serverWeb.arg(dhcpWiFi) == on)
            {
                doc[dhcpWiFi] = 1;
            }
            else
            {
                doc[dhcpWiFi] = 0;
            }
            doc["ip"] = serverWeb.arg("ipAddress");
            doc["mask"] = serverWeb.arg("ipMask");
            doc["gw"] = serverWeb.arg("ipGW");
            configFile = LittleFS.open(configFileWifi, FILE_WRITE);
            serializeJson(doc, configFile);
            configFile.close();
            WiFi.persistent(false);
            if (ConfigSettings.apStarted)
            {
                WiFi.mode(WIFI_AP_STA);
            }
            else
            {
                WiFi.mode(WIFI_STA);
            }
            WiFi.begin(serverWeb.arg("WIFISSID").c_str(), serverWeb.arg("WIFIpassword").c_str());
        }
        break;
        case API_PAGE_ZHA_Z2M:
        {
            configFile = LittleFS.open(configFileSerial, FILE_READ);
            deserializeJson(doc, configFile);
            configFile.close();
            const char *baud = "baud";
            if (serverWeb.hasArg(baud))
            {
                doc[baud] = serverWeb.arg(baud);
            }
            else
            {
                doc[baud] = 115200;
            }
            const char *port = "port";
            if (serverWeb.hasArg(baud))
            {
                doc[port] = serverWeb.arg(port);
            }
            else
            {
                doc[port] = 6638;
            }
            configFile = LittleFS.open(configFileSerial, FILE_WRITE);
            serializeJson(doc, configFile);
            configFile.close();
        }
        break;
        case API_PAGE_SECURITY:
        {
            configFile = LittleFS.open(configFileSecurity, FILE_READ);
            deserializeJson(doc, configFile);
            configFile.close();
            const char *disableWeb = "disableWeb";
            if (serverWeb.arg(disableWeb) == on)
            {
                doc[disableWeb] = 1;
            }
            else
            {
                doc[disableWeb] = 0;
            }
            const char *webAuth = "webAuth";
            if (serverWeb.arg(webAuth) == on)
            {
                doc[webAuth] = 1;
            }
            else
            {
                doc[webAuth] = 0;
            }
            const char *webUser = "webUser";
            if (serverWeb.arg(webUser) != "")
            {
                doc[webUser] = serverWeb.arg(webUser);
            }
            else
            {
                doc[webUser] = "admin";
            }
            const char *fwEnabled = "fwEnabled";
            if (serverWeb.arg(fwEnabled) == on)
            {
                doc[fwEnabled] = 1;
            }
            else
            {
                doc[fwEnabled] = 0;
            }
            const char *fwIp = "fwIp";
            doc[fwIp] = serverWeb.arg(fwIp);
            doc["webPass"] = serverWeb.arg("webPass");

            configFile = LittleFS.open(configFileSecurity, FILE_WRITE);
            serializeJson(doc, configFile);
            configFile.close();
        }
        break;
        case API_PAGE_SYSTOOLS:
        {
            const char *refreshLogs = "refreshLogs";
            const char *hostname = "hostname";
            configFile = LittleFS.open(configFileGeneral, FILE_READ);
            deserializeJson(doc, configFile);
            configFile.close();
            if (serverWeb.hasArg(refreshLogs))
            {
                ConfigSettings.refreshLogs = serverWeb.arg(refreshLogs).toInt();
                doc[refreshLogs] = ConfigSettings.refreshLogs;
            }
            if (serverWeb.hasArg(hostname))
            {
                doc[hostname] = serverWeb.arg(hostname);
                strlcpy(ConfigSettings.hostname, serverWeb.arg(hostname).c_str(), sizeof(ConfigSettings.hostname));
            }
            const char *timeZoneName = "timeZoneName";
            if (serverWeb.hasArg(timeZoneName))
            {
                doc[timeZoneName] = serverWeb.arg(timeZoneName);
            }
            configFile = LittleFS.open(configFileGeneral, FILE_WRITE);
            configFile = LittleFS.open(configFileGeneral, FILE_WRITE);
            serializeJson(doc, configFile);
            configFile.close();
        }
        break;

        default:
            break;
        }
        serverWeb.send(HTTP_CODE_OK, contTypeText, "ok");
    }
    else
    {
        serverWeb.send(500, contTypeText, "bad args");
    }
}

void handleSavefile()
{
    if (checkAuth())
    {
        if (serverWeb.method() != HTTP_POST)
        {
            serverWeb.send(405, contTypeText, F("Method Not Allowed"));
        }
        else
        {
            String filename = "/config/" + serverWeb.arg(0);
            String content = serverWeb.arg(1);
            File file = LittleFS.open(filename, "w");
            if (!file)
            {
                DEBUG_PRINT(F("Failed to open file for reading\r\n"));
                return;
            }

            int bytesWritten = file.print(content);

            if (bytesWritten > 0)
            {
                DEBUG_PRINTLN(F("File was written"));
                DEBUG_PRINTLN(bytesWritten);
            }
            else
            {
                DEBUG_PRINTLN(F("File write failed"));
            }

            file.close();
            serverWeb.sendHeader(F("Location"), F("/sys-tools"));
            serverWeb.send(303);
        }
    }
}
