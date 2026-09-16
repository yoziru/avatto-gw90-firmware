#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <HTTPClient.h>

#include "config.h"
#include "web.h"
#include "etc.h"
#include "net.h"
#include "wifi_save_logic.h"

extern bool loadConfigMqtt();
extern bool loadConfigWg();

// Candidate wifi config that is only committed to disk after the STA link to
// it actually succeeds (see wifiCommitPending). Kept in RAM so a wrong-password
// join can never overwrite the saved network.
struct PendingWifi
{
    bool active = false;
    unsigned long started = 0;
    String ssid;
    String pass;
    bool dhcp;
    String ip;
    String mask;
    String gw;
};

static PendingWifi pendingWifi;

// Persist the wifi fields to /config/configWifi.json and refresh runtime
// ConfigSettings so the new values are active without a reboot.
static void writeWifiConfig(const char *ssid, const char *pass, bool dhcp, const char *ip, const char *mask, const char *gw)
{
    File configFile = LittleFS.open(configFileWifi, FILE_READ);
    JsonDocument doc;
    deserializeJson(doc, configFile);
    configFile.close();
    doc["ssid"] = ssid;
    doc["pass"] = pass;
    doc["dhcpWiFi"] = dhcp ? 1 : 0;
    doc["ip"] = ip;
    doc["mask"] = mask;
    doc["gw"] = gw;
    configFile = LittleFS.open(configFileWifi, FILE_WRITE);
    serializeJson(doc, configFile);
    configFile.close();
    loadConfigWifi();
}

static void wifiStagePending(const char *ssid, const char *pass, bool dhcp, const char *ip, const char *mask, const char *gw)
{
    pendingWifi.active = true;
    pendingWifi.started = millis();
    pendingWifi.ssid = ssid;
    pendingWifi.pass = pass;
    pendingWifi.dhcp = dhcp;
    pendingWifi.ip = ip;
    pendingWifi.mask = mask;
    pendingWifi.gw = gw;
}

// Called from the main loop: once the STA is connected to the very network we
// staged, commit it to disk. A join that never connects (wrong password, AP
// out of range) gives up after a timeout: the staged config is dropped, the
// retry loop is stopped, and the previously saved network is left intact
// (the overseer brings it back up when WiFi is the active uplink).
void wifiCommitPending()
{
    if (!pendingWifi.active)
    {
        return;
    }
    const bool connected = (WiFi.status() == WL_CONNECTED);
    const String staSsid = WiFi.SSID();
    const bool toPending = wifiShouldCommit(pendingWifi.active, connected, staSsid.c_str(), pendingWifi.ssid.c_str());
    if (toPending)
    {
        writeWifiConfig(pendingWifi.ssid.c_str(), pendingWifi.pass.c_str(), pendingWifi.dhcp,
                        pendingWifi.ip.c_str(), pendingWifi.mask.c_str(), pendingWifi.gw.c_str());
        pendingWifi.active = false;
        printLogMsg("WiFi: saved '" + pendingWifi.ssid + "' after successful connection");
        return;
    }
    if (!wifiPendingGiveUp(toPending, connected && staSsid != pendingWifi.ssid, millis() - pendingWifi.started))
    {
        return;
    }
    pendingWifi.active = false;
    if (connected)
    {
        // The saved network won the race: stay on it, just note the miss.
        printLogMsg("WiFi: join to '" + pendingWifi.ssid + "' abandoned, staying on '" + staSsid + "'");
    }
    else
    {
        // Stop retrying the staged SSID. With Ethernet up the radio goes idle;
        // otherwise the disconnect event restarts the overseer, which retries
        // the saved network.
        WiFi.disconnect();
        if (ConfigSettings.connectedEther)
        {
            WiFi.mode(WIFI_OFF);
        }
        printLogMsg("WiFi: could not connect to '" + pendingWifi.ssid + "' - kept saved network");
    }
}

void handleSaveParams()
{
    if (!checkAuth())
        return;
    String result;
    JsonDocument doc;
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
                if (mode <= 1 && mode >= zero)
                {
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
            const char *refreshLogs = "refreshLogs";
            const char *hostname = "hostname";
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

            const char *tls = "tls";
            doc[tls] = serverWeb.arg("MqttTls") == "on" ? one : zero;

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
            doc["localPrivKey"] = serverWeb.arg("WgLocalPrivKey");
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
            const String ssidArg = serverWeb.arg("WIFISSID");
            const String passArg = serverWeb.arg("WIFIpassword");
            const bool dhcp = (serverWeb.arg("dhcpWiFi") == on);
            const String ipArg = serverWeb.arg("ipAddress");
            const String maskArg = serverWeb.arg("ipMask");
            const String gwArg = serverWeb.arg("ipGW");

            const WifiSaveMode mode = wifiSaveMode(ssidArg.c_str(), ConfigSettings.ssid);

            if (mode == WifiSaveMode::Forget)
            {
                // Forget: clear the saved network and drop any active STA link.
                writeWifiConfig("", "", dhcp, ipArg.c_str(), maskArg.c_str(), gwArg.c_str());
                pendingWifi.active = false;
                WiFi.disconnect();
                if (ConfigSettings.apStarted)
                {
                    WiFi.mode(WIFI_AP_STA);
                }
                else
                {
                    WiFi.mode(WIFI_STA);
                }
                break;
            }

            if (mode == WifiSaveMode::SaveNow)
            {
                // The saved network itself is being tweaked (IP settings etc.):
                // persist immediately, no validation needed.
                writeWifiConfig(ssidArg.c_str(), passArg.c_str(), dhcp, ipArg.c_str(), maskArg.c_str(), gwArg.c_str());
                pendingWifi.active = false;
            }
            else
            {
                // NEW network: stage it in RAM, connect, and only commit to disk
                // once the STA link to it actually succeeds (see wifiCommitPending).
                // A wrong password or unreachable AP can never become the saved
                // network this way.
                wifiStagePending(ssidArg.c_str(), passArg.c_str(), dhcp, ipArg.c_str(), maskArg.c_str(), gwArg.c_str());
                printLogMsg("WiFi: staged '" + ssidArg + "' pending connect");
            }

            connectWifiSta(ssidArg.c_str(), passArg.c_str(), dhcp, ipArg.c_str(), maskArg.c_str(), gwArg.c_str());
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
            if (serverWeb.hasArg(port))
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

        default:
            break;
        }
        printLogMsg(String("Websave page ") + serverWeb.arg(pageId));
        serverWeb.send(HTTP_CODE_OK, contTypeText, "ok");
    }
    else
    {
        serverWeb.send(500, contTypeText, "bad args");
    }
}
