#include <Arduino.h>
#include <ArduinoJson.h>
#include <ETH.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <LittleFS.h>
#include <WiFi.h>

#include <esp_task_wdt.h>

#include "config.h"
#include "encoding.h"
#include "web.h"
#include "log.h"
#include "etc.h"
#include "zb.h"
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

void handleZigbeeBSL()
{
    if (checkAuth())
    {
        zigbeeEnableBSL();
        serverWeb.send(HTTP_CODE_OK, contTypeText, "");
    }
}

void handleZigbeeRestart()
{
    if (checkAuth())
    {
        zigbeeRestart();
        serverWeb.send(HTTP_CODE_OK, contTypeText, "");
    }
}

void handleApi()
{
    enum API_ACTION_t : uint8_t
    {
        API_GET_PAGE,
        API_GET_PARAM,
        API_STARTWIFISCAN,
        API_WIFISCANSTATUS,
        API_GET_FILELIST,
        API_GET_FILE,
        API_SEND_HEX,
        API_WIFICONNECTSTAT,
        API_CMD,
        API_GET_LOG,
        API_FLASH_ZB
    };
    const char *action = "action";
    const char *page = "page";
    const char *Authentication = "Authentication";
    const char *param = "param";
    const char *wrongArgs = "wrong args";
    const char *ok = "ok";

    if (ConfigSettings.webAuth)
    {
        if (!checkAuth())
        {
            serverWeb.sendHeader(Authentication, "fail");
            serverWeb.send(401, contTypeText, F("wrong login or password"));
            return;
        }
        else
        {
            serverWeb.sendHeader(Authentication, ok);
        }
    }
    if (serverWeb.argName(0) != action)
    {
        DEBUG_PRINT(F("[handleApi] wrong arg 'action' "));
        DEBUG_PRINTLN(serverWeb.argName(0));
        serverWeb.send(500, contTypeText, wrongArgs);
    }
    else
    {
        const uint8_t actionId = serverWeb.arg(action).toInt();
        switch (actionId)
        {
        case API_FLASH_ZB:
        {
            Serial.println("\n\n========== api_update zigbee =========");
            ConfigSettings.zbFlashing = 1;
            const char *fwurlArg = "fwurl";
            const char *tagZB_FW_info = "ZB_FW_info";
            const char *tagZB_FW_err = "ZB_FW_err";
            const char *tagZB_FW_download = "ZB_FW_downloading";
            const char *tagZB_FW_flash = "ZB_FW_flashing";

            if (serverWeb.hasArg(fwurlArg))
            {
                String fwUrl = serverWeb.arg(fwurlArg);
                fwUrl = urlDecode(fwUrl);

                Serial.print("Firmware URL: ");
                Serial.println(fwUrl);

                bool wifiConnected = (WiFi.status() == WL_CONNECTED);
                bool ethConnected = ETH.linkUp();

                if (!wifiConnected && !ethConnected) {
                    Serial.println("No network connection!");
                    serverWeb.send(HTTP_CODE_SERVICE_UNAVAILABLE, contTypeText, "No network connection");
                    sendEvent(tagZB_FW_err, sizeof(tagZB_FW_err), "No network connection!");
                    ConfigSettings.zbFlashing = 0;
                    break;
                }

                serverWeb.send(HTTP_CODE_OK, contTypeText, ok);

                uint8_t evWaitCount = 0;
                while (!eventsClient.connected() && evWaitCount < 20)
                {
                    webServerHandleClient();
                    delay(50);
                    evWaitCount++;
                }

                if (!eventsClient.connected()) {
                    Serial.println("Events client not ready, using polling mode");
                }

                HTTPClient http;
                NetworkClientSecure client;
                client.setInsecure();
                client.setTimeout(10000);

                http.begin(client, fwUrl);
                http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
                http.addHeader("Accept", "*/*");
                http.addHeader("Accept-Encoding", "identity");
                http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
                http.setTimeout(60000);

                Serial.println("Connecting to firmware server...");
                const int16_t httpCode = http.GET();

                int32_t fwSize = http.getSize();

                if (httpCode != HTTP_CODE_OK)
                {
                    Serial.print("HTTP Error: ");
                    Serial.println(httpCode);
                    sendEvent(tagZB_FW_err, sizeof(tagZB_FW_err), "HTTP Error: " + String(httpCode));
                    http.end();
                    ConfigSettings.zbFlashing = 0;
                    break;
                }

                Serial.println("Connection successful!");

                LittleFS.remove(tempFile);

                File fwFile = LittleFS.open(tempFile, "w");
                if (!fwFile)
                {
                    sendEvent(tagZB_FW_err, sizeof(tagZB_FW_err), "Failed to open file for writing");
                    ConfigSettings.zbFlashing = 0;
                    http.end();
                    break;
                }
                sendEvent(tagZB_FW_info, sizeof(tagZB_FW_info), "[start]");
                sendEvent(tagZB_FW_info, sizeof(tagZB_FW_info), "Downloading firmware...");

                WiFiClient *stream = http.getStreamPtr();
                uint8_t buff[1024];
                uint32_t downloaded = 0;
                unsigned long lastDataMs = millis();
                unsigned long lastProgressTime = 0;
                uint8_t percent = 0;
                // getSize() returns -1/0 when the server omits Content-Length;
                // then we download until the stream ends instead of trying to
                // compute percentages (which would divide by zero).
                const bool knownSize = (fwSize > 0);

                while (http.connected() || stream->available())
                {
                    size_t available = stream->available();

                    if (knownSize && downloaded >= (uint32_t)fwSize)
                        break;

                    if (available > 0)
                    {
                        size_t toRead = available;
                        if (toRead > sizeof(buff)) {
                            toRead = sizeof(buff);
                        }

                        int bytesRead = stream->read(buff, toRead);

                        if (bytesRead > 0)
                        {
                            fwFile.write(buff, bytesRead);
                            downloaded += bytesRead;
                            lastDataMs = millis();

                            unsigned long now = millis();
                            if (knownSize && ((now - lastProgressTime > 500) || (downloaded % 10240 == 0))) {
                                percent = ((float)downloaded / fwSize) * 100;
                                sendEvent(tagZB_FW_download, sizeof(tagZB_FW_download), String(percent));
                                lastProgressTime = now;
                            }
                        }
                    }
                    else
                    {
                        if (millis() - lastDataMs > 200000) {
                            Serial.println("Download timeout!");
                            sendEvent(tagZB_FW_err, sizeof(tagZB_FW_err), "Download timeout!");
                            fwFile.close();
                            ConfigSettings.zbFlashing = 0;
                            http.end();
                            return;
                        }

                        delay(10);
                    }

                    static uint16_t loopCount = 0;
                    if (++loopCount % 10 == 0) {
                        webServerHandleClient();
                    }
                }
                fwFile.close();
                http.end();

                if (!knownSize || (int32_t)downloaded == fwSize)
                {
                    if (knownSize)
                    {
                        percent = ((float)downloaded / fwSize) * 100;
                        sendEvent(tagZB_FW_download, sizeof(tagZB_FW_download), String(percent));
                    }

                    sendEvent(tagZB_FW_info, sizeof(tagZB_FW_info), "Download complete!");
                    sendEvent(tagZB_FW_info, sizeof(tagZB_FW_info), String("Total size: ") + String(downloaded) + " bytes");
                    sendEvent(tagZB_FW_info, sizeof(tagZB_FW_info), "Validating and flashing...");

                    xTaskCreatePinnedToCore(checkFwHexTask, "CheckFWHex", 8192, (void *)tempFile, 3, NULL, 0);
                }
            }
            else
            {
                serverWeb.send(HTTP_CODE_BAD_REQUEST, contTypeText, "missing arg 1");
            }
            ConfigSettings.zbFlashing = 0;
        }
        break;
        case API_GET_LOG:
        {
            String result;
            result = logPrint();
            serverWeb.send(HTTP_CODE_OK, contTypeText, result);
        }
        break;
        case API_CMD:
        {
            enum CMD_t : uint8_t
            {
                CMD_ZB_ROUTER_RECON = 0,
                CMD_ZB_RST = 1,
                CMD_ZB_BSL = 2,
                CMD_ESP_RES = 3,
                // 4, 5 were the deprecated temporary mode switches (removed);
                // numbers are kept so existing data-cmd / API callers don't shift.
                CMD_LED_PWR_TOG = 6,
                CMD_LED_USB_TOG = 7,
                CMD_CLEAR_LOG = 8,
                CMD_ESP_UPD_URL = 9,
                CMD_ZB_CHK_REV = 10,
                CMD_ZB_CHK_CON = 11,
                CMD_ZB_LED_TOG = 12
            };
            String result = wrongArgs;
            const char *argCmd = "cmd";
            const char *argUrl = "url";
            if (serverWeb.hasArg(argCmd))
            {
                result = "ok";
                switch (serverWeb.arg(argCmd).toInt())
                {
                case CMD_CLEAR_LOG:
                    logClear();
                    break;
                case CMD_ZB_ROUTER_RECON:
                    zigbeeRouterRejoin();
                    break;
                case CMD_ZB_RST:
                    zigbeeRestart();
                    break;
                case CMD_ZB_BSL:
                    zigbeeEnableBSL();
                    break;
                case CMD_ESP_RES:
                    serverWeb.send(HTTP_CODE_OK, contTypeText, result);
                    ESP.restart();
                    break;
                case CMD_LED_PWR_TOG:
                    ledPowerToggle();
                    break;
                case CMD_LED_USB_TOG:
                    ledUSBToggle();
                    break;
                case CMD_ESP_UPD_URL:
                    if (serverWeb.hasArg(argUrl))
                        getEspUpdate(serverWeb.arg(argUrl));
                    else
                        getEspUpdate(UPD_FILE);
                    break;
                case CMD_ZB_CHK_REV:
                    getZbVer();
                    break;
                case CMD_ZB_CHK_CON:
                    zbCheck();
                    break;
                case CMD_ZB_LED_TOG:
                    zbLedToggle();
                    break;
                default:
                    break;
                }
                serverWeb.send(HTTP_CODE_OK, contTypeText, result);
            }
        }
        break;
        case API_WIFICONNECTSTAT:
        {
            String result;
            JsonDocument doc;
            const char *connected = "connected";
            if (WiFi.status() == WL_CONNECTED)
            {
                doc[connected] = true;
                doc["ip"] = WiFi.localIP().toString();
                doc["mask"] = WiFi.subnetMask().toString();
                doc["gw"] = WiFi.gatewayIP().toString();
                doc["ssid"] = WiFi.SSID();
            }
            else
            {
                doc[connected] = false;
            }
            serializeJson(doc, result);
            serverWeb.send(HTTP_CODE_OK, contTypeJson, result);
        }
        break;
        case API_GET_FILE:
        {
            String result = wrongArgs;
            const char *argFilename = "filename";
            if (serverWeb.hasArg(argFilename))
            {
                String filename = "/config/" + serverWeb.arg(argFilename);
                File file = LittleFS.open(filename, "r");
                if (!file)
                {
                    serverWeb.send(HTTP_CODE_NOT_FOUND, contTypeText, "not found");
                    break;
                }
                result = "";
                while (file.available())
                {
                    result += (char)file.read();
                }
                file.close();
            }
            serverWeb.send(HTTP_CODE_OK, contTypeText, result);
        }
        break;
        case API_GET_PARAM:
        {
            String resp = wrongArgs;
            if (serverWeb.hasArg(param))
            {
                if (serverWeb.arg(param) == "refreshLogs")
                {
                    resp = (String)ConfigSettings.refreshLogs;
                }
                else if (serverWeb.arg(param) == "coordMode")
                {
                    resp = String(static_cast<uint8_t>(ConfigSettings.coordinator_mode));
                }
                else if (serverWeb.arg(param) == "zbRev")
                {
                    resp = zbVer.zbRev > 0 ? (String)zbVer.zbRev : "Unknown";
                }
                else if (serverWeb.arg(param) == "espVer")
                {
                    resp = VERSION;
                }
            }
            serverWeb.send(HTTP_CODE_OK, contTypeText, resp);
        }
        break;
        case API_STARTWIFISCAN:
            if (WiFi.getMode() == WIFI_OFF)
            {
                WiFi.mode(WIFI_STA);
            }
            WiFi.scanNetworks(true);
            serverWeb.send(HTTP_CODE_OK, contTypeTextHtml, ok);
            break;
        case API_WIFISCANSTATUS:
        {
            static uint8_t timeout = 0;
            JsonDocument doc;
            String result = "";
            int16_t scanRes = WiFi.scanComplete();
            const char *scanDone = "scanDone";
            doc[scanDone] = false;
            if (scanRes == -2)
            {
                WiFi.scanNetworks(true);
            }
            else if (scanRes > 0)
            {
                doc[scanDone] = true;
                JsonArray wifi = doc["wifi"].to<JsonArray>();
                for (int i = 0; i < scanRes; ++i)
                {
                    JsonObject wifi_0 = wifi.add<JsonObject>();
                    wifi_0["ssid"] = WiFi.SSID(i);
                    wifi_0["rssi"] = WiFi.RSSI(i);
                    wifi_0["channel"] = WiFi.channel(i);
                    wifi_0["secure"] = WiFi.encryptionType(i);
                }
                WiFi.scanDelete();
            }
            if (timeout < 10)
            {
                timeout++;
            }
            else
            {
                doc[scanDone] = true;
                WiFi.scanDelete();
            }
            if(doc[scanDone] == true)timeout = 0;
            serializeJson(doc, result);
            serverWeb.send(HTTP_CODE_OK, contTypeJson, result);
            break;
        }
        case API_GET_PAGE:
            if (!serverWeb.arg(page).length() > 0)
            {
                DEBUG_PRINTLN(F("[handleApi] wrong arg 'page'"));
                DEBUG_PRINTLN(serverWeb.argName(1));
                serverWeb.send(500, contTypeText, wrongArgs);
                return;
            }
            switch (serverWeb.arg(page).toInt())
            {
            case API_PAGE_ROOT:
                handleRoot();
                sendGzip(contTypeTextHtml, PAGE_ROOT_html_gz, PAGE_ROOT_html_gz_len);
                break;
            case API_PAGE_GENERAL:
                handleGeneral();
                sendGzip(contTypeTextHtml, PAGE_GENERAL_html_gz, PAGE_GENERAL_html_gz_len);
                break;
            case API_PAGE_ETHERNET:
                handleEther();
                sendGzip(contTypeTextHtml, PAGE_ETHERNET_html_gz, PAGE_ETHERNET_html_gz_len);
                break;
            case API_PAGE_WIFI:
                handleWifi();
                sendGzip(contTypeTextHtml, PAGE_WIFI_html_gz, PAGE_WIFI_html_gz_len);
                break;
            case API_PAGE_ZHA_Z2M:
                handleSerial();
                sendGzip(contTypeTextHtml, PAGE_SERIAL_html_gz, PAGE_SERIAL_html_gz_len);
                break;
            case API_PAGE_SECURITY:
                handleSecurity();
                sendGzip(contTypeTextHtml, PAGE_SECURITY_html_gz, PAGE_SECURITY_html_gz_len);
                break;
            case API_PAGE_SYSTOOLS:
                handleSysTools();
                sendGzip(contTypeTextHtml, PAGE_SYSTOOLS_html_gz, PAGE_SYSTOOLS_html_gz_len);
                break;
            case API_PAGE_ABOUT:
                handleAbout();
                sendGzip(contTypeTextHtml, PAGE_ABOUT_html_gz, PAGE_ABOUT_html_gz_len);
                break;
            case API_PAGE_MQTT:
                handleMqtt();
                sendGzip(contTypeTextHtml, PAGE_MQTT_html_gz, PAGE_MQTT_html_gz_len);
                break;
            case API_PAGE_WG:
                handleWg();
                sendGzip(contTypeTextHtml, PAGE_WG_html_gz, PAGE_WG_html_gz_len);
                break;
            default:
                break;
            }
            break;
        case API_GET_FILELIST:
        {
            String fileList = "";
            JsonDocument doc;
            JsonArray files = doc["files"].to<JsonArray>();
            File root = LittleFS.open("/config");
            File file = root.openNextFile();
            while (file)
            {
                JsonObject jsonfile = files.add<JsonObject>();
                jsonfile["filename"] = String(file.name());
                jsonfile["size"] = file.size();
                file = root.openNextFile();
            }
            serializeJson(doc, fileList);
            serverWeb.send(HTTP_CODE_OK, contTypeJson, fileList);
            break;
        }

        default:
            DEBUG_PRINTLN(F("[handleApi] switch (action) err"));
            break;
        }
    }
}
